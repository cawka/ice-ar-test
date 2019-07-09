/* -*- Mode:jde; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

package net.named_data.ice_ar;

import android.annotation.SuppressLint;
import android.graphics.Color;
import android.os.Bundle;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.BaseAdapter;
import android.widget.ListView;
import android.widget.TextView;

import java.util.ArrayList;
import java.util.HashMap;

import androidx.annotation.Keep;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;

public class LogcatFragment extends Fragment implements NdnRtcWrapper.Logger {
  private static final String TAG = LogcatFragment.class.getName();

  @Override
  public void onCreate(Bundle savedInstanceState)
  {
    super.onCreate(savedInstanceState);
    setHasOptionsMenu(true);
  }

  @Override
  public View onCreateView(LayoutInflater inflater,
                           @Nullable ViewGroup container,
                           @Nullable Bundle savedInstanceState)
  {
    @SuppressLint("InflateParams")
    View v = inflater.inflate(R.layout.fragment_logcat_output, null);

    String[] levels = getResources().getStringArray(R.array.loglevel_names);
    int[] foregrounds = getResources().getIntArray(R.array.foreground_colors);
    int[] backgrounds = getResources().getIntArray(R.array.background_colors);
    HashMap<String, ColorsItem> colorMap = new HashMap<>();
    for (int i = 0; i < Math.min(Math.min(levels.length, foregrounds.length), backgrounds.length); ++i) {
      colorMap.put(levels[i], new ColorsItem(foregrounds[i], backgrounds[i]));
    }

    // Get UI Elements
    m_logListView = (ListView) v.findViewById(R.id.log_output);
    m_logListAdapter = new LogListAdapter(inflater, s_logMaxLines, colorMap,
                                          new ColorsItem(R.color.foreground_debug, R.color.background_debug));
    m_logListView.setAdapter(m_logListAdapter);

    m_logListView.setOnItemClickListener((AdapterView<?> parent, View view, int position, long id) -> {
      TextView level = (TextView)view.findViewById(R.id.log_level_text);
      TextView msg = (TextView)view.findViewById(R.id.log_line);

      LogListAdapter.Item item = (LogListAdapter.Item) m_logListAdapter.getItem(position);

      int lineCount = msg.getLineCount();
      if (item.lines == lineCount) {
        item.lines = 1;
      }
      else {
        item.lines = lineCount;
      }

      msg.setMaxLines(item.lines);
      msg.setMinLines(item.lines);

      level.setMaxLines(item.lines);
      level.setMinLines(item.lines);
    });

    return v;
  }

  @Override
  public void onResume()
  {
    super.onResume();
    NdnRtcWrapper.attach(this);
  }

  @Override
  public void onPause()
  {
    super.onPause();
    NdnRtcWrapper.detach(this);
  }

  @Keep @Override
  public void
  addMessageFromNative(String module, String severity, String message)
  {
    getActivity().runOnUiThread(() -> {
      if (severity.equals("TRACE")) {
        // suppress all trace stuff
        return;
      }
      if (module.equals("ndn.Face") && severity.equals("DEBUG")) {
        if (message.contains("/localhost/nfd")) {
          // ignore all face exchanges to/from local NFD
          return;
        }
      }
      appendLogText(module, severity, message);
    });
  }

  //////////////////////////////////////////////////////////////////////////////

  /**
   * Clear log adapter and update UI.
   */
  public void clearLog()
  {
    m_logListAdapter.clearMessages();
  }

  /**
   * Convenience method to append a message to the log output
   * and scroll to the bottom of the log.
   *
   * @param message String message to be posted to the text view.
   */
  private void appendLogText(String module, String severity, String message) {
    m_logListAdapter.addMessage(module, severity, message);
    m_logListView.setSelection(m_logListAdapter.getCount() - 1);
  }

  /**
   * Custom LogListAdapter to limit the number of log lines that
   * is being stored and displayed.
   */
  private static class LogListAdapter extends BaseAdapter {

    /**
     * Create a ListView compatible adapter with an
     * upper bound on the maximum number of entries that will
     * be displayed in the ListView.
     *
     * @param maxLines Maximum number of entries allowed in
     *                 the ListView for this adapter.
     */
    LogListAdapter(LayoutInflater layoutInflater, int maxLines,
                   HashMap<String, ColorsItem> colorMap, ColorsItem defaultColor) {
      m_data = new ArrayList<>();
      m_layoutInflater = layoutInflater;
      m_maxLines = maxLines;
      m_colorMap = colorMap;
      m_defaultColor = defaultColor;
    }

    /**
     * Add a message to be displayed in the log's list view.
     *
     * @param message Message to be added to the underlying data store
     *                and displayed on thi UI.
     */
    void addMessage(String module, String level, String message) {
      if (m_data.size() == m_maxLines) {
        m_data.remove(0);
      }

      m_data.add(new Item(module, level, message));
      notifyDataSetChanged();
    }

    /**
     * Convenience method to clear all messages from the underlying
     * data store and update the UI.
     */
    void clearMessages() {
      m_data.clear();
      this.notifyDataSetChanged();
    }

    @Override
    public int getCount() {
      return m_data.size();
    }

    @Override
    public Object getItem(int position) {
      return m_data.get(position);
    }

    @Override
    public long getItemId(int position) {
      return position;
    }

    @SuppressLint("InflateParams")
    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
      LogEntryViewHolder holder;

      if (convertView == null) {
        holder = new LogEntryViewHolder();

        convertView = m_layoutInflater.inflate(R.layout.list_item_log, null);
        convertView.setTag(holder);

        holder.logLineTextView = (TextView) convertView.findViewById(R.id.log_line);
        holder.logLevel = convertView.findViewById(R.id.log_level_text);
      } else {
        holder = (LogEntryViewHolder) convertView.getTag();
      }

      Item item = m_data.get(position);
      holder.logLineTextView.setText(item.message);
      holder.logLineTextView.setMaxLines(item.lines);
      holder.logLineTextView.setMinLines(item.lines);
      holder.logLevel.setText(item.level.substring(0, 1));
      holder.logLevel.setMaxLines(item.lines);
      holder.logLevel.setMinLines(item.lines);
      ColorsItem color = m_colorMap.get(item.level);
      if (color == null) {
        color = m_defaultColor;
      }
      holder.logLevel.setBackgroundColor(color.background);
      holder.logLevel.setTextColor(color.foreground);
      return convertView;
    }

    class Item {
      Item(final String module, final String level, final String message)
      {
        this.module = module;
        this.level = level;
        this.message = message;
        this.lines = 1;
      }

      String module;
      String level;
      String message;
      int lines;
    }

    /** Underlying message data store for log messages*/
    private final ArrayList<Item> m_data;

    /** Layout inflater for inflating views */
    private final LayoutInflater m_layoutInflater;

    /** Maximum number of log lines to display */
    private final int m_maxLines;

    private HashMap<String, ColorsItem> m_colorMap;
    ColorsItem m_defaultColor;
  }

  /**
   * Log entry view holder object for holding the output.
   */
  private static class LogEntryViewHolder {
    TextView logLevel;
    TextView logLineTextView;
  }

  //////////////////////////////////////////////////////////////////////////////

  /** Maximum number of log lines to be displayed by the backing adapter of the ListView */
  private static final int s_logMaxLines = 380;

  /** Process in which logcat is running in */
  private Process m_logProcess;

  /** ListView for displaying log output in */
  private ListView m_logListView;

  /** Customized ListAdapter for controlling log output */
  private LogListAdapter m_logListAdapter;

  class ColorsItem {
    ColorsItem(int foreground, int background)
    {
      this.foreground = foreground;
      this.background = background;
    }

    int foreground;
    int background;
  }
}
