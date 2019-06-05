package net.named_data.ice_ar;

import java.util.Map;

/**
 * NfdService that runs the native NFD.
 * <p>
 * NfdService runs as an independent process within the Android OS that provides
 * service level features to start and stop the NFD native code through the
 * NFD JNI wrapper.
 */
public class NdnRtcWrapper {
  static {
    System.loadLibrary("ndnrtc-wrapper");
  }

  /**
   * Native API for starting the NFD.
   * <p/>
   * @param params NFD parameters.  Must include 'homePath' with absolute path of the home directory
   *               for the service (ContextWrapper.getFilesDir().getAbsolutePath())
   */
  public native static void
  test(Map<String, String> params);
}
