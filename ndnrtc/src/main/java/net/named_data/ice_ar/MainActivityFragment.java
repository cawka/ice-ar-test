package net.named_data.ice_ar;

import androidx.fragment.app.Fragment;

import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Trace;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;

import java.io.File;
import java.util.HashMap;
import java.util.Map;

/**
 * A placeholder fragment containing a simple view.
 */
public class MainActivityFragment extends Fragment {

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
                             Bundle savedInstanceState) {

        View view = inflater.inflate(R.layout.fragment_main, container, false);

        Button button = view.findViewById(R.id.button);

        button.setOnClickListener((View v) -> {
            HashMap<String, String> params = new HashMap<>();
            File dir = getActivity().getFilesDir();
            if (dir != null) {
                params.put("homePath", getActivity().getFilesDir().getAbsolutePath());
            }
            new SimpleTask().execute(params);
        });

        return view;
    }

    private class SimpleTask extends AsyncTask<HashMap<String,String>, Void, Void> {
        @Override
        protected void onPreExecute() {
            super.onPreExecute();
        }

        @Override
        protected Void doInBackground(HashMap<String,String>... params) {
            Log.println(Log.INFO, "NdnRtcWrapper", "fafa");
            NdnRtcWrapper.test(params[0]);
            Log.println(Log.INFO, "NdnRtcWrapper", "fufu");
            return null;
        }
    }
}
