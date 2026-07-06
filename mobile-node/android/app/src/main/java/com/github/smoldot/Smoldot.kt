package com.github.smoldot

import kotlinx.coroutines.flow.Flow
import kotlin.concurrent.atomics.AtomicReference
import kotlin.concurrent.atomics.ExperimentalAtomicApi
import kotlin.concurrent.atomics.updateAndFetch

@OptIn(ExperimentalAtomicApi::class)
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

    public interface Chain {
        public val id: Int

        /**
         * JSON-RPC responses and notifications emitted by the chain. Collecting fails with
         * [SmoldotPanicException] if the client panics.
         */
        public val jsonRpcResponses: Flow<String>

        public suspend fun sendJsonRpc(request: String)
        public suspend fun close()
    }

    public enum class LogLevel(internal val value: UInt) {
        Error(1u),
        Warn(2u),
        Info(3u),
        Debug(4u),
        Trace(5u),
    }

    public companion object {
        private var INSTANCE: AtomicReference<Smoldot?> = AtomicReference(null)
        private var factory: AtomicReference<(() -> Smoldot)?> = AtomicReference(null)

        internal fun useInstance(factory: () -> Smoldot) {
            this.factory.store(factory)
        }

        public fun instance(): Smoldot =
            INSTANCE.updateAndFetch { it ?: factory.load()?.invoke() }
                ?: throw IllegalStateException("Smoldot not initialized. Call platform init method first.")

        public suspend fun reset() {
            INSTANCE.exchange(null)?.destroy()
        }
    }
}