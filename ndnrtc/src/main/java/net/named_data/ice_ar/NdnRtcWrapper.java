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
  attach(LogcatFragment logcat);

  public native static void
  detach(LogcatFragment logcat);
}
