package com.github.smoldot

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import kotlinx.coroutines.CoroutineStart
import kotlinx.coroutines.async
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.withTimeout
import org.json.JSONObject
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import kotlin.time.Duration.Companion.seconds

@RunWith(AndroidJUnit4::class)
class SmoldotAndroidTest {

    @Before
    fun setUp() {
        Smoldot.initAndroid(Smoldot.LogLevel.Info)
    }

    @After
    fun tearDown() {
        runBlocking {
            runCatching { Smoldot.reset() }
        }
    }

    @Test
    fun addsChainAndInitializes() = runBlocking {
        val chain = withTimeout(INIT_TIMEOUT) {
            Smoldot.instance().addChain(readAsset(CHAIN_SPEC))
        }

        assertTrue(chain.id >= 0)
    }

    @Test
    fun answersChainNameRequestLocally() = runBlocking {
        val spec = readAsset(CHAIN_SPEC)
        val expectedName = JSONObject(spec).getString("name")
        val chain = withTimeout(INIT_TIMEOUT) {
            Smoldot.instance().addChain(spec)
        }

        val response = jsonRpcRoundTrip(
            chain,
            """{"jsonrpc":"2.0","id":1,"method":"chainSpec_v1_chainName"}""",
        )

        val json = JSONObject(response)
        assertEquals(1, json.getInt("id"))
        assertEquals(expectedName, json.getString("result"))
    }

    @Test
    fun rejectsInvalidChainSpec() = runBlocking {
        val result = runCatching {
            withTimeout(INIT_TIMEOUT) {
                Smoldot.instance().addChain("not a valid chain spec")
            }
        }

        assertTrue("addChain should reject an invalid chain spec", result.isFailure)
    }

    @Test
    fun removesChain() = runBlocking {
        val smoldot = Smoldot.instance()
        val chain = withTimeout(INIT_TIMEOUT) {
            smoldot.addChain(readAsset(CHAIN_SPEC))
        }

        withTimeout(INIT_TIMEOUT) { smoldot.removeChain(chain) }
    }

    @Test
    fun throwsOnPanicAndRecoversAfterReset() = runBlocking {
        val smoldot = Smoldot.instance()
        val chain = withTimeout(INIT_TIMEOUT) {
            smoldot.addChain(readAsset(CHAIN_SPEC))
        }

        // Sending a request to a removed chain violates the client's API contract and makes it
        // panic - a convenient way to exercise the panic recovery path without any test hooks.
        withTimeout(INIT_TIMEOUT) { smoldot.removeChain(chain) }
        val panic = runCatching { chain.sendJsonRpc(CHAIN_NAME_REQUEST) }.exceptionOrNull()
        assertTrue("expected SmoldotPanicException, got: $panic", panic is SmoldotPanicException)

        // The client keeps failing fast until it is reset.
        val subsequent = runCatching { chain.sendJsonRpc(CHAIN_NAME_REQUEST) }.exceptionOrNull()
        assertTrue("expected SmoldotPanicException, got: $subsequent", subsequent is SmoldotPanicException)

        // A full reset restores a working client.
        Smoldot.reset()
        val recovered = withTimeout(INIT_TIMEOUT) {
            Smoldot.instance().addChain(readAsset(CHAIN_SPEC))
        }
        val response = jsonRpcRoundTrip(recovered, CHAIN_NAME_REQUEST)

        assertTrue(JSONObject(response).has("result"))
    }

    private suspend fun jsonRpcRoundTrip(chain: Smoldot.Chain, request: String): String =
        withTimeout(RESPONSE_TIMEOUT) {
            coroutineScope {
                // Start undispatched so the collector subscribes (and suspends) before the request
                // is sent; responses are not replayed to late subscribers.
                val response = async(start = CoroutineStart.UNDISPATCHED) {
                    chain.jsonRpcResponses.first()
                }
                chain.sendJsonRpc(request)
                response.await()
            }
        }

    private fun readAsset(name: String): String {
        val context = InstrumentationRegistry.getInstrumentation().context
        return context.assets.open(name).bufferedReader().use { it.readText() }
    }

    private companion object {
        const val CHAIN_SPEC = "westend2.json"

        const val CHAIN_NAME_REQUEST = """{"jsonrpc":"2.0","id":1,"method":"chainSpec_v1_chainName"}"""

        val INIT_TIMEOUT = 60.seconds
        val RESPONSE_TIMEOUT = 30.seconds
    }
}
