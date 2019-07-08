package net.named_data.ice_ar;

import android.content.Context;
import android.net.wifi.SupplicantState;
import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import android.os.Bundle;
import android.util.Log;
import android.view.View;

import com.google.android.material.floatingactionbutton.FloatingActionButton;

import java.io.File;
import java.util.HashMap;

import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.Toolbar;

public class MainActivity extends AppCompatActivity implements NdnRtcWrapper.StartStopNotify {
  private String TAG = MainActivity.class.getName();
  private boolean m_isStartAction = true;
  private FloatingActionButton m_button;
  private LogcatFragment m_logFragment;

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    setContentView(R.layout.activity_main);
    Toolbar toolbar = findViewById(R.id.toolbar);
    setSupportActionBar(toolbar);

    m_logFragment = (LogcatFragment)getSupportFragmentManager().findFragmentById(R.id.fragment);

    m_button = findViewById(R.id.fab);
    m_button.setOnClickListener((View v) -> {
      if (m_isStartAction) {
        m_logFragment.clearLog();
        HashMap<String, String> params = new HashMap<>();
        File dir = getFilesDir();
        if (dir != null) {
          params.put("homePath", getFilesDir().getAbsolutePath());
          params.put("log", "ndncert.*=ALL:ndn.Face=ALL");
        }
        NdnRtcWrapper.start(params, this);
      }
      else {
        NdnRtcWrapper.stop();
      }
      m_button.setClickable(false);
    });
  }

  @Override
  public void onStarted()
  {
    runOnUiThread(() -> {
      m_isStartAction = false;
      m_button.setImageResource(android.R.drawable.ic_media_pause);
      m_button.setClickable(true);
    });
  }

  @Override
  public void onStopped()
  {
    runOnUiThread(() -> {
      m_isStartAction = true;
      m_button.setImageResource(android.R.drawable.ic_media_play);
      m_button.setClickable(true);
    });
  }

  @Override
  public String getWifi()
  {
    WifiManager wifiManager = (WifiManager)getApplicationContext().getSystemService(Context.WIFI_SERVICE);
    WifiInfo wifiInfo;

    wifiInfo = wifiManager.getConnectionInfo();
    if (wifiInfo.getSupplicantState() == SupplicantState.COMPLETED) {
      return wifiInfo.getBSSID();
    }
    else {
      return "";
    }
  }
}
