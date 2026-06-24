# JNI native methods are bound to C++ by their fully-qualified name (Java_com_github_smoldot_*)
# at load time. Keep the declaring class and its native methods so R8 cannot rename them.
-keepclasseswithmembernames,includedescriptorclasses class com.github.smoldot.SmoldotAndroid {
    native <methods>;
}

# Invoked from native code via JNI GetMethodID by name; must not be renamed or removed.
-keepclassmembers class com.github.smoldot.SmoldotAndroid {
    public void onChainInitialized(long, java.lang.String);
    public void notifyChain(long);
}
