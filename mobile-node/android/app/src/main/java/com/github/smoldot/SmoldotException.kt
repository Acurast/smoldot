package com.github.smoldot

public sealed class SmoldotException(message: String?, cause: Throwable?) : Throwable(message, cause)

public class SmoldotInitializationException(message: String? = null, cause: Throwable? = null) : SmoldotException(message, cause)
public class SmoldotRpcException(message: String? = null, cause: Throwable? = null) : SmoldotException(message, cause)