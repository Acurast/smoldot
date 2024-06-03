package com.github.smoldot

import com.github.smoldot.internal.utils.MutableSharedMapFlow
import kotlinx.coroutines.CloseableCoroutineDispatcher
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.asSharedFlow
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.launch
import kotlinx.coroutines.newSingleThreadContext
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.withContext
import java.nio.ByteBuffer
import java.nio.ByteOrder
import kotlin.math.log
import kotlin.math.min

public class SmoldotAndroid(logLevel: Smoldot.LogLevel) : Smoldot {
    private val coroutineScope: CoroutineScope = CoroutineScope(SupervisorJob() + Dispatchers.Default)

    private val chainResults: MutableSharedMapFlow<Int, Result<Unit>> = MutableSharedMapFlow()

    private val chainsMutex: Mutex = Mutex()
    private val chains: MutableMap<Int, Chain> = mutableMapOf()

    init {
        jniInit(logLevel.value.toLong())
    }

    override suspend fun addChain(
        chainSpec: String,
        databaseContent: String?,
        potentialRelayChains: List<Smoldot.Chain>,
        disableJsonRpc: Boolean,
        jsonRpcMaxPendingRequests: UInt,
        jsonRpcMaxSubscriptions: UInt,
    ): Smoldot.Chain = withContext(Dispatchers.IO) {
        val chainId = jniAddChain(
            chainSpec.toByteArray(charset = Charsets.UTF_8),
            (databaseContent ?: "").toByteArray(charset = Charsets.UTF_8),
            potentialRelayChains.map {
                ByteBuffer.allocate(4).apply {
                    order(ByteOrder.LITTLE_ENDIAN)
                    putInt(it.id)
                }.array()
            }.fold(byteArrayOf(), ByteArray::plus),
            if (disableJsonRpc) 0 else min(jsonRpcMaxPendingRequests.toLong(), 0xffffffff),
            min(jsonRpcMaxSubscriptions.toLong(), 0xffffffff),
        )

        val chainResult = chainResults[chainId.toInt()].first()

        val chain = if (chainResult.isSuccess) Chain(chainId.toInt()) else run {
            jniRemoveChain(chainId)
            throw chainResult.exceptionOrNull() ?: RuntimeException("Chain failed to initialize due to an unknown error.")
        }

        chain.also {
            chainsMutex.withLock {
                chains[it.id] = it
            }
        }
    }

    public fun onChainInitialized(chainId: Long, error: String?) {
        coroutineScope.launch {
            val chainId = chainId.toInt()
            chainResults.waitUntilSubscribed(chainId)
            chainResults.emit(chainId, error?.let { Result.failure(RuntimeException(it)) } ?: Result.success(Unit))
        }
    }

    public fun notifyChain(chainId: Long) {
        coroutineScope.launch {
            chainsMutex.withLock { chains[chainId.toInt()]?.onNonEmptyJsonRpcResponses() }
        }
    }

    override suspend fun removeChain(chain: Smoldot.Chain) {
         withContext(Dispatchers.IO) {
            chain.close()
            chainsMutex.withLock {
                chains.remove(chain.id)
            }
        }
    }

    override suspend fun destroy() {
        chainsMutex.withLock {
            with(chains) {
                values.forEach { it.close() }
                clear()
            }
        }
        coroutineScope.cancel()

        jniDestroy()
    }

    public inner class Chain(override val id: Int) : Smoldot.Chain {
        private val jsonRpcResponsesContext: CloseableCoroutineDispatcher = newSingleThreadContext("Chain\$$id-json-rpc-responses")
        private var jsonRpcResponsesNonEmpty: CompletableDeferred<Unit>? = null
        private val _jsonRpcResponses: MutableSharedFlow<String> = MutableSharedFlow()

        override val jsonRpcResponses: Flow<String>
            get() = _jsonRpcResponses.asSharedFlow()

        init {
            CoroutineScope(jsonRpcResponsesContext).launch {
                while (true) {
                    val response = jniJsonRpcResponsesPeek(id.toUInt().toLong())

                    if (response != null) {
                        _jsonRpcResponses.emit(response.toString(charset = Charsets.UTF_8))
                    }

                    if (response == null) {
                        jsonRpcResponsesNonEmpty = CompletableDeferred()
                        jsonRpcResponsesNonEmpty?.await()
                        jsonRpcResponsesNonEmpty = null
                    }
                }
            }
        }

        override suspend fun sendJsonRpc(request: String) = withContext(Dispatchers.IO) {
            when (val result = jniSendJsonRpc(request.toByteArray(charset = Charsets.UTF_8), id.toUInt().toLong())) {
                0L -> return@withContext
                1L -> throw RuntimeException("JSON-RPC requests queue is full")
                else -> throw RuntimeException("Internal error: unknown json_rpc_send error code: $result")
            }
        }

        internal fun onNonEmptyJsonRpcResponses() {
            CoroutineScope(jsonRpcResponsesContext).launch {
                jsonRpcResponsesNonEmpty?.complete(Unit)
            }
        }

        override fun close() {
            jsonRpcResponsesContext.close()
            jniRemoveChain(id.toUInt().toLong())
        }

    }

    private external fun jniInit(logLevel: Long)

    private external fun jniAddChain(
        chainSpec: ByteArray,
        databaseContent: ByteArray,
        potentialRelayChains: ByteArray,
        jsonRpcMaxPendingRequests: Long,
        jsonRpcMaxSubscriptions: Long,
    ): Long
    private external fun jniRemoveChain(chainId: Long)

    private external fun jniSendJsonRpc(request: ByteArray, chainId: Long): Long
    private external fun jniJsonRpcResponsesPeek(chainId: Long): ByteArray?
    private external fun jniDestroy()
}

public fun Smoldot.Companion.initAndroid(logLevel: Smoldot.LogLevel = Smoldot.LogLevel.Info) {
    System.loadLibrary("smoldot")
    useInstance { SmoldotAndroid(logLevel) }
}