package com.github.smoldot

import kotlinx.coroutines.runBlocking
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Test

class SmoldotTest {

    private class FakeSmoldot : Smoldot {
        var destroyed: Boolean = false

        override suspend fun addChain(
            chainSpec: String,
            databaseContent: String?,
            potentialRelayChains: List<Smoldot.Chain>,
            disableJsonRpc: Boolean,
            jsonRpcMaxPendingRequests: UInt,
            jsonRpcMaxSubscriptions: UInt,
        ): Smoldot.Chain = throw UnsupportedOperationException()

        override suspend fun removeChain(chain: Smoldot.Chain) = throw UnsupportedOperationException()

        override suspend fun destroy() {
            destroyed = true
        }
    }

    @After
    fun tearDown() {
        runBlocking {
            runCatching { Smoldot.reset() }
        }
    }

    @Test
    fun `instance - lazily creates from the factory and caches the result`() {
        var created = 0
        val fake = FakeSmoldot()
        Smoldot.useInstance {
            created++
            fake
        }

        val first = Smoldot.instance()
        val second = Smoldot.instance()

        assertSame(fake, first)
        assertSame(first, second)
        assertEquals(1, created)
    }

    @Test
    fun `reset - destroys the current instance`() = runBlocking {
        val fake = FakeSmoldot()
        Smoldot.useInstance { fake }
        Smoldot.instance()

        Smoldot.reset()

        assertTrue(fake.destroyed)
    }

    @Test
    fun `instance - recreates a fresh instance after reset`() = runBlocking {
        val first = FakeSmoldot()
        val second = FakeSmoldot()
        val queue = ArrayDeque(listOf(first, second))
        Smoldot.useInstance { queue.removeFirst() }

        assertSame(first, Smoldot.instance())
        Smoldot.reset()
        assertSame(second, Smoldot.instance())
    }
}
