package com.github.smoldot

import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import java.io.Closeable

public interface Smoldot {
    public suspend fun addChain(
        chainSpec: String,
        databaseContent: String? = null,
        potentialRelayChains: List<Chain> = emptyList(),
        disableJsonRpc: Boolean = false,
        jsonRpcMaxPendingRequests: UInt = UInt.MAX_VALUE,
        jsonRpcMaxSubscriptions: UInt = UInt.MAX_VALUE,
    ): Chain
    public suspend fun removeChain(chain: Chain)

    public suspend fun destroy()

    public interface Chain : Closeable {
        public val id: Int
        public val jsonRpcResponses: Flow<String>

        public suspend fun sendJsonRpc(request: String)
    }

    public enum class LogLevel(internal val value: UInt) {
        Error(1u),
        Warn(2u),
        Info(3u),
        Debug(4u),
        Trace(5u),
    }

    public companion object {
        private val instanceMutex: Mutex = Mutex()
        private var INSTANCE: Smoldot? = null

        private var factory: (() -> Smoldot)? = null

        internal fun useInstance(factory: () -> Smoldot) {
            this.factory = factory
        }

        public suspend fun instance(): Smoldot = instanceMutex.withLock {
            INSTANCE
                ?: factory?.invoke()?.also { INSTANCE = it }
                ?: throw IllegalStateException("Smoldot not initialized. Call platform init method first.")
        }

        public suspend fun reset() {
            instanceMutex.withLock {
                INSTANCE?.destroy()
                INSTANCE = null
            }
        }
    }
}