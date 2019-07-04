package net.named_data.ice_ar;

import android.os.Bundle;
import android.view.View;

import com.google.android.material.floatingactionbutton.FloatingActionButton;

import java.io.File;
import java.util.HashMap;

import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.Toolbar;

public class MainActivity extends AppCompatActivity {
  private boolean isStartAction = true;

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    setContentView(R.layout.activity_main);
    Toolbar toolbar = findViewById(R.id.toolbar);
    setSupportActionBar(toolbar);

    FloatingActionButton btn = findViewById(R.id.fab);
    btn.setOnClickListener((View v) -> {
      if (isStartAction) {
        HashMap<String, String> params = new HashMap<>();
        File dir = getFilesDir();
        if (dir != null) {
          params.put("homePath", getFilesDir().getAbsolutePath());
          params.put("log", "*=ALL");
        }
        NdnRtcWrapper.start(params);
      }
      else {
        NdnRtcWrapper.stop();
      }
      isStartAction = !isStartAction;
    });
  }
}
