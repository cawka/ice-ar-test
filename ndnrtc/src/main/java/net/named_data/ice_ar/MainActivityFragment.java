package net.named_data.ice_ar;

import androidx.fragment.app.Fragment;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;

import java.io.File;
import java.util.HashMap;

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
            NdnRtcWrapper.test(params);
        });

        return view;
    }
}
