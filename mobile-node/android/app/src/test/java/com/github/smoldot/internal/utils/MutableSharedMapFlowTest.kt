package com.github.smoldot.internal.utils

import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.launch
import kotlinx.coroutines.test.UnconfinedTestDispatcher
import kotlinx.coroutines.test.advanceUntilIdle
import kotlinx.coroutines.test.runTest
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

@OptIn(ExperimentalCoroutinesApi::class)
class MutableSharedMapFlowTest {

    @Test
    fun `get - delivers only values emitted for the matching key`() = runTest(UnconfinedTestDispatcher()) {
        val flow = MutableSharedMapFlow<Int, String>()
        val received = mutableListOf<String>()

        val collector = backgroundScope.launch { flow[1].collect { received += it } }
        flow.waitUntilSubscribed(1)

        flow.emit(1, "a")
        flow.emit(2, "b")
        flow.emit(1, "c")
        advanceUntilIdle()

        assertEquals(listOf("a", "c"), received)
        collector.cancel()
    }

    @Test
    fun `get - separate keys receive their own values independently`() = runTest(UnconfinedTestDispatcher()) {
        val flow = MutableSharedMapFlow<Int, String>()
        val one = mutableListOf<String>()
        val two = mutableListOf<String>()

        val c1 = backgroundScope.launch { flow[1].collect { one += it } }
        val c2 = backgroundScope.launch { flow[2].collect { two += it } }
        flow.waitUntilSubscribed(1)
        flow.waitUntilSubscribed(2)

        flow.emit(1, "a")
        flow.emit(2, "b")
        advanceUntilIdle()

        assertEquals(listOf("a"), one)
        assertEquals(listOf("b"), two)
        c1.cancel()
        c2.cancel()
    }

    @Test
    fun `waitUntilSubscribed - suspends until a collector subscribes to that key`() = runTest(UnconfinedTestDispatcher()) {
        val flow = MutableSharedMapFlow<Int, String>()
        var resumed = false

        val waiter = launch {
            flow.waitUntilSubscribed(7)
            resumed = true
        }
        advanceUntilIdle()
        assertFalse(resumed)

        val collector = backgroundScope.launch { flow[7].collect { } }
        advanceUntilIdle()
        assertTrue(resumed)

        waiter.join()
        collector.cancel()
    }
}
