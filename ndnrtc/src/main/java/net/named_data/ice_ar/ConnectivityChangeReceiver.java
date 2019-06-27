package net.named_data.ice_ar;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.util.Log;

/**
 * ConnectivityChangeReceiver monitors network connectivity changes and update
 * face and route list accordingly.
 */
public class ConnectivityChangeReceiver extends BroadcastReceiver {
  private static final String TAG = ConnectivityChangeReceiver.class.getName();

  @Override
  public void
  onReceive(Context context, Intent intent) {
    NetworkInfo network = getNetworkInfo(context);
    if (null == network) {
      Log.d(TAG, "Connection lost");
      onConnectionLost(context);
    } else {
      Log.d(TAG, "Connection changed");
      onChange(context, network);
    }
  }

  private static NetworkInfo getNetworkInfo(Context applicationContext) {
    ConnectivityManager cm = (ConnectivityManager) applicationContext
        .getSystemService(Context.CONNECTIVITY_SERVICE);
    return cm.getActiveNetworkInfo();
  }

  private void
  onChange(Context context, NetworkInfo networkInfo) {
    if (networkInfo.isConnected()) {
      Log.d(TAG, "Network connectivity has changed connected");

      // @TODO Request new cert
//      context.startService(new Intent(context, NfdService.class));
    }
  }

  private void
  onConnectionLost(Context context) {
    // nothing to do
  }
}
