package ir.arsacia.agrak;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.IBinder;
import android.os.PowerManager;
import androidx.annotation.Nullable;
import androidx.core.app.NotificationCompat;
import org.json.JSONArray;
import org.json.JSONObject;
import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.net.HttpURLConnection;
import java.net.URL;
import java.net.URLEncoder;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.CopyOnWriteArrayList;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class BotService extends Service {
    public static final String CHANNEL_ID = "AGRak_Channel";
    public static final int NOTIFICATION_ID = 1001;
    public static final String REMOTE_SERVER_URL = "http://78.135.77.95:8080";

    public interface LogCallback {
        void onNewLog(String accountId, String line);
        void onStatsUpdated(String accountId, String level, String respect, String rob, String escape);
    }

    public static class RemoteAccount {
        public String nick;
        public boolean online;
        public String level;
        public String respect;
        public String rob;
        public String escape;
    }

    public interface RemoteAccountsListener {
        void onResult(boolean success, List<RemoteAccount> accounts);
    }

    public interface RemoteLogsListener {
        void onResult(boolean success, List<String> lines);
    }

    public interface RemoteStatsListener {
        void onStatsResult(boolean success, String level, String respect, String rob, String escape, List<String> logs);
    }

    private static final List<LogCallback> logCallbacks = new CopyOnWriteArrayList<>();
    private static final Map<String, StringBuilder> accountLogs = Collections.synchronizedMap(new HashMap<>());
    private static final Map<String, String[]> cachedStats = Collections.synchronizedMap(new HashMap<>());
    private PowerManager.WakeLock wakeLock;

    private static final Pattern PATTERN_LEVEL_RESPECT = Pattern.compile("Level\\s*\\[(\\d+)\\]\\s*Respect\\s*\\[([0-9/]+)\\]", Pattern.CASE_INSENSITIVE);
    private static final Pattern PATTERN_ROB_ESCAPE = Pattern.compile("Rob points\\s*\\[(\\d+)\\]\\s*Escape points\\s*\\[(\\d+)\\]", Pattern.CASE_INSENSITIVE);

    public static void addLogCallback(LogCallback callback) {
        if (callback != null && !logCallbacks.contains(callback)) {
            logCallbacks.add(callback);
        }
    }

    public static void removeLogCallback(LogCallback callback) {
        if (callback != null) {
            logCallbacks.remove(callback);
        }
    }

    public static void setLogCallback(LogCallback callback) {
        logCallbacks.clear();
        if (callback != null) {
            logCallbacks.add(callback);
        }
    }

    public static String getLogsForAccount(String accountId) {
        StringBuilder sb = accountLogs.get(accountId);
        return sb != null ? sb.toString() : "";
    }

    public static void appendLog(String accountId, String line) {
        StringBuilder sb = accountLogs.computeIfAbsent(accountId, k -> new StringBuilder());
        sb.append(line).append("\n");

        parseLineForStats(accountId, line);

        for (LogCallback cb : logCallbacks) {
            try {
                cb.onNewLog(accountId, line);
            } catch (Exception ignored) {}
        }
    }

    public static void parseLineForStats(String accountId, String line) {
        if (line == null) return;
        String[] stats = cachedStats.computeIfAbsent(accountId, k -> new String[]{"--", "--", "--", "--"});
        boolean changed = false;

        Matcher m1 = PATTERN_LEVEL_RESPECT.matcher(line);
        if (m1.find()) {
            stats[0] = m1.group(1);
            stats[1] = m1.group(2);
            changed = true;
        }

        Matcher m2 = PATTERN_ROB_ESCAPE.matcher(line);
        if (m2.find()) {
            stats[2] = m2.group(1);
            stats[3] = m2.group(2);
            changed = true;
        }

        if (changed) {
            for (LogCallback cb : logCallbacks) {
                try {
                    cb.onStatsUpdated(accountId, stats[0], stats[1], stats[2], stats[3]);
                } catch (Exception ignored) {}
            }
        }
    }

    public static void updateStats(String accountId, String level, String respect, String rob, String escape) {
        String[] stats = cachedStats.computeIfAbsent(accountId, k -> new String[]{"--", "--", "--", "--"});
        stats[0] = level;
        stats[1] = respect;
        stats[2] = rob;
        stats[3] = escape;

        for (LogCallback cb : logCallbacks) {
            try {
                cb.onStatsUpdated(accountId, level, respect, rob, escape);
            } catch (Exception ignored) {}
        }
    }

    public static void fetchRemoteAccountsAsync(RemoteAccountsListener listener) {
        new Thread(() -> {
            try {
                String urlStr = REMOTE_SERVER_URL + "/api/accounts";
                URL url = new URL(urlStr);
                HttpURLConnection conn = (HttpURLConnection) url.openConnection();
                conn.setConnectTimeout(3000);
                conn.setReadTimeout(3000);
                conn.setRequestMethod("GET");

                if (conn.getResponseCode() == 200) {
                    BufferedReader in = new BufferedReader(new InputStreamReader(conn.getInputStream()));
                    StringBuilder sb = new StringBuilder();
                    String line;
                    while ((line = in.readLine()) != null) {
                        sb.append(line);
                    }
                    in.close();

                    JSONObject obj = new JSONObject(sb.toString());
                    if ("ok".equals(obj.optString("status"))) {
                        JSONArray arr = obj.optJSONArray("accounts");
                        List<RemoteAccount> list = new ArrayList<>();
                        if (arr != null) {
                            for (int i = 0; i < arr.length(); i++) {
                                JSONObject accObj = arr.getJSONObject(i);
                                RemoteAccount ra = new RemoteAccount();
                                ra.nick = accObj.optString("nick", "");
                                ra.online = accObj.optBoolean("online", false);
                                ra.level = accObj.optString("level", "--");
                                ra.respect = accObj.optString("respect", "--");
                                ra.rob = accObj.optString("rob", "--");
                                ra.escape = accObj.optString("escape", "--");
                                list.add(ra);
                            }
                        }
                        if (listener != null) {
                            listener.onResult(true, list);
                        }
                        return;
                    }
                }
            } catch (Exception ignored) {}

            if (listener != null) {
                listener.onResult(false, Collections.emptyList());
            }
        }).start();
    }

    public static void fetchRemoteLogsAsync(String nickname, RemoteLogsListener listener) {
        if (nickname == null) return;
        new Thread(() -> {
            try {
                String urlStr = REMOTE_SERVER_URL + "/api/logs?nick=" + URLEncoder.encode(nickname, "UTF-8");
                URL url = new URL(urlStr);
                HttpURLConnection conn = (HttpURLConnection) url.openConnection();
                conn.setConnectTimeout(3000);
                conn.setReadTimeout(3000);
                conn.setRequestMethod("GET");

                if (conn.getResponseCode() == 200) {
                    BufferedReader in = new BufferedReader(new InputStreamReader(conn.getInputStream()));
                    StringBuilder sb = new StringBuilder();
                    String line;
                    while ((line = in.readLine()) != null) {
                        sb.append(line);
                    }
                    in.close();

                    JSONObject obj = new JSONObject(sb.toString());
                    if ("ok".equals(obj.optString("status"))) {
                        JSONArray arr = obj.optJSONArray("lines");
                        List<String> lines = new ArrayList<>();
                        if (arr != null) {
                            for (int i = 0; i < arr.length(); i++) {
                                lines.add(arr.getString(i));
                            }
                        }
                        if (listener != null) {
                            listener.onResult(true, lines);
                        }
                        return;
                    }
                }
            } catch (Exception ignored) {}

            if (listener != null) {
                listener.onResult(false, Collections.emptyList());
            }
        }).start();
    }

    public static void fetchLiveStatsAsync(String nickname, RemoteStatsListener listener) {
        new Thread(() -> {
            try {
                String urlStr = REMOTE_SERVER_URL + "/api/stats?nick=" + URLEncoder.encode(nickname, "UTF-8");
                URL url = new URL(urlStr);
                HttpURLConnection conn = (HttpURLConnection) url.openConnection();
                conn.setConnectTimeout(2500);
                conn.setReadTimeout(2500);
                conn.setRequestMethod("GET");

                if (conn.getResponseCode() == 200) {
                    BufferedReader in = new BufferedReader(new InputStreamReader(conn.getInputStream()));
                    StringBuilder sb = new StringBuilder();
                    String line;
                    while ((line = in.readLine()) != null) {
                        sb.append(line);
                    }
                    in.close();

                    JSONObject obj = new JSONObject(sb.toString());
                    if ("ok".equals(obj.optString("status"))) {
                        JSONObject acc = obj.getJSONObject("account");
                        String level = acc.optString("level", "--");
                        String respect = acc.optString("respect", "--");
                        String rob = acc.optString("rob", "--");
                        String escape = acc.optString("escape", "--");
                        if (listener != null) {
                            listener.onStatsResult(true, level, respect, rob, escape, null);
                        }
                        return;
                    }
                }
            } catch (Exception ignored) {}

            if (listener != null) {
                listener.onStatsResult(false, "--", "--", "--", "--", null);
            }
        }).start();
    }

    public static void startRemoteBot(String nickname, String password) {
        if (nickname == null) return;
        new Thread(() -> {
            try {
                String urlStr = REMOTE_SERVER_URL + "/api/start?nick=" + URLEncoder.encode(nickname, "UTF-8")
                        + (password != null ? "&pass=" + URLEncoder.encode(password, "UTF-8") : "");
                URL url = new URL(urlStr);
                HttpURLConnection conn = (HttpURLConnection) url.openConnection();
                conn.setConnectTimeout(2500);
                conn.setReadTimeout(2500);
                conn.setRequestMethod("POST");
                conn.getResponseCode();
            } catch (Exception ignored) {}
        }).start();
    }

    public static void stopRemoteBot(String nickname) {
        if (nickname == null) return;
        new Thread(() -> {
            try {
                String urlStr = REMOTE_SERVER_URL + "/api/stop?nick=" + URLEncoder.encode(nickname, "UTF-8");
                URL url = new URL(urlStr);
                HttpURLConnection conn = (HttpURLConnection) url.openConnection();
                conn.setConnectTimeout(2500);
                conn.setReadTimeout(2500);
                conn.setRequestMethod("POST");
                conn.getResponseCode();
            } catch (Exception ignored) {}
        }).start();
    }

    public static void sendRemoteCommand(String nickname, String cmd) {
        if (nickname == null || cmd == null) return;
        new Thread(() -> {
            try {
                String urlStr = REMOTE_SERVER_URL + "/api/cmd?nick=" + URLEncoder.encode(nickname, "UTF-8")
                        + "&cmd=" + URLEncoder.encode(cmd, "UTF-8");
                URL url = new URL(urlStr);
                HttpURLConnection conn = (HttpURLConnection) url.openConnection();
                conn.setConnectTimeout(2000);
                conn.setReadTimeout(2000);
                conn.setRequestMethod("POST");
                conn.getResponseCode();
            } catch (Exception ignored) {}
        }).start();
    }

    @Override
    public void onCreate() {
        super.onCreate();
        createNotificationChannel();
        acquireWakeLock();
        startForeground(NOTIFICATION_ID, createNotification("AGRak در حال اجرا است"));
    }

    private void acquireWakeLock() {
        PowerManager pm = (PowerManager) getSystemService(Context.POWER_SERVICE);
        if (pm != null) {
            wakeLock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "AGRak:BotWakeLock");
            wakeLock.acquire();
        }
    }

    private void createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            NotificationChannel channel = new NotificationChannel(
                    CHANNEL_ID,
                    "سرویس فعال راک‌سمپ AGRak",
                    NotificationManager.IMPORTANCE_LOW
            );
            channel.setDescription("اجرای مداوم ربات در پس‌زمینه بدون قطعی");
            NotificationManager manager = getSystemService(NotificationManager.class);
            if (manager != null) {
                manager.createNotificationChannel(channel);
            }
        }
    }

    public Notification createNotification(String content) {
        Intent notificationIntent = new Intent(this, MainActivity.class);
        PendingIntent pendingIntent = PendingIntent.getActivity(
                this, 0, notificationIntent,
                PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT
        );

        return new NotificationCompat.Builder(this, CHANNEL_ID)
                .setContentTitle("AGRak • آرساکیا گیم")
                .setContentText(content)
                .setSmallIcon(R.mipmap.ic_launcher)
                .setContentIntent(pendingIntent)
                .setOngoing(true)
                .build();
    }

    public void updateNotificationContent(int activeCount) {
        String msg = activeCount > 0
                ? activeCount + " اکانت آنلاین در سرور 185.112.33.99"
                : "آماده اتصال به سرور آرساکیا";
        NotificationManager manager = (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
        if (manager != null) {
            manager.notify(NOTIFICATION_ID, createNotification(msg));
        }
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        return START_STICKY;
    }

    @Override
    public void onDestroy() {
        if (wakeLock != null && wakeLock.isHeld()) {
            wakeLock.release();
        }
        super.onDestroy();
    }

    @Nullable
    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }
}
