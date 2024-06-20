package com.github.smoldot.internal.utils

import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.channels.BufferOverflow
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.filter
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.flow.onCompletion
import kotlinx.coroutines.flow.onSubscription
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock

internal interface MutableSharedMapFlow<K, V> : MutableSharedFlow<Pair<K, V>> {
    operator fun get(key: K): Flow<V>
    suspend fun emit(key: K, value: V)

    suspend fun waitUntilSubscribed(key: K)
}

private class MutableSharedMapFlowImpl<K, V> (
    private val innerFlow: MutableSharedFlow<Pair<K, V>>
) : MutableSharedMapFlow<K, V>, MutableSharedFlow<Pair<K, V>> by innerFlow {
    private val subscribedMutex = Mutex()
    private val subscribed: MutableMap<K, CompletableDeferred<Unit>> = mutableMapOf()

    override fun get(key: K): Flow<V> = innerFlow
        .onSubscription { with(subscribedMutex) { subscribed.complete(key) } }
        .onCompletion { with(subscribedMutex) { subscribed.clear(key)} }
        .filter { it.first == key }
        .map { it.second }

    override suspend fun emit(key: K, value: V) {
        emit(Pair(key, value))
    }

    override suspend fun waitUntilSubscribed(key: K) {
        with(subscribedMutex) { subscribed.await(key) }
    }

    context(Mutex)
    private suspend fun MutableMap<K, CompletableDeferred<Unit>>.complete(key: K) {
        withLock { getOrPutNew(key) }.takeIf { !it.isCompleted }?.complete(Unit)
    }

    context(Mutex)
    private suspend fun MutableMap<K, CompletableDeferred<Unit>>.clear(key: K) {
        withLock { remove(key) }
    }

    context(Mutex)
    private suspend fun MutableMap<K, CompletableDeferred<Unit>>.await(key: K) {
        withLock { getOrPutNew(key) }.await()
    }

    private fun MutableMap<K, CompletableDeferred<Unit>>.getOrPutNew(key: K): CompletableDeferred<Unit> =
        getOrPut(key) { CompletableDeferred() }
}

internal fun <K, V> MutableSharedMapFlow(
    replay: Int = 0,
    extraBufferCapacity: Int = 0,
    onBufferOverflow: BufferOverflow = BufferOverflow.SUSPEND
): MutableSharedMapFlow<K, V> = MutableSharedMapFlowImpl(MutableSharedFlow(replay, extraBufferCapacity, onBufferOverflow))