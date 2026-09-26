# Release shrinking rules.

# The JNI symbol names are derived from the exact class + method names of
# org.tetra.receiver.TetraNative, so this class must not be renamed.
-keep class org.tetra.receiver.TetraNative { *; }

# Custom View inflated from the layout XML by reflection.
-keep class org.tetra.receiver.WaterfallView { *; }
