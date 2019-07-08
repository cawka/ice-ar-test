package net.named_data.ice_ar;

import java.util.Map;

public class NdnRtcWrapper {
  static {
    System.loadLibrary("ice-ar-wrapper");
  }

  public interface StartStopNotify {
    void
    onStarted();

    void
    onStopped();

    String
    getWifi();
  }

  public interface Logger {
    void
    addMessageFromNative(String module, String severity, String message);
  }

  /**
   * Native API
   * <p/>
   * @param params NFD parameters.  Must include 'homePath' with absolute path of the home directory
   *               for the service (ContextWrapper.getFilesDir().getAbsolutePath())
   */
  public native static void
  start(Map<String, String> params, StartStopNotify notify);

  public native static void
  stop();

  public native static void
  attach(Logger logger);

  public native static void
  detach(Logger logger);
}
