# ICEAR Security Demo app

## Building from source

### Installing Android SDK and NDK

- Option 1: Using Android Studio

Download Android Studio and install SDK components.  You MUST also select NDK (ndk-bundle) to be installed.

By default, on macOS, SDK will be installed into `/Users/<user-name>/Library/Android/sdk` and `/home/<user-name>/Android/Sdk` on Linux.  You can choose to install it in other location.  NDK is installed relative to SDK to `ndk-bundle` folder (`/Users/<user-name>/Library/Android/sdk/ndk-bundle` or  `/home/<user-name>/Android/Sdk/ndk-bundle`).

- Option 2: Just using command-line

        mkdir android-sdk-linux

        # On Linux
        SDK=sdk-tools-linux-4333796.zip

        # On macOS
        SDK=sdk-tools-darwin-4333796.zip

        wget ttps://dl.google.com/android/repository/$SDK
        unzip $SDK
        rm $SDK

        export ANDROID_HOME=`pwd`
        export PATH=${PATH}:${ANDROID_HOME}/tools/bin:${ANDROID_HOME}/platform-tools
        echo "y" | sdkmanager "platform-tools"
        sdkmanager "platforms;android-28" "ndk-bundle"

### Installing precompiled binaries

    cd <sdk-path>/ndk-bundle
    git clone https://github.com/named-data-mobile/android-crew-staging crew.dir

    CREW_OWNER=named-data-mobile crew.dir/crew install target/ndn_cxx target/ndncert_guest

### Preparing build

For command-line build, you will also need to create `local.properties` file with paths to SDK and NDK, for example:

    ndk.dir=/opt/android/ndk-bundle
    sdk.dir=/opt/android

### Building

From command line:

    ./gradlew assembleDebug
    # or ./gradlew assembleRelease (more configuration and proper keys required)

You can also build from Android Studio in the usual way.
