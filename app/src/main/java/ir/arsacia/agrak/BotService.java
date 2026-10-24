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
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

public class BotService extends Service {
    public static final String CHANNEL_ID = "AGRak_Channel";
    public static final int NOTIFICATION_ID = 1001;

    public interface LogCallback {
        void onNewLog(String accountId, String line);
        void onStatsUpdated(String accountId, String level, String respect, String rob, String escape);
    }

    private static LogCallback logCallback;
    private static final Map<String, StringBuilder> accountLogs = Collections.synchronizedMap(new HashMap<>());
    private PowerManager.WakeLock wakeLock;

    public static void setLogCallback(LogCallback callback) {
        logCallback = callback;
    }

    public static String getLogsForAccount(String accountId) {
        StringBuilder sb = accountLogs.get(accountId);
        return sb != null ? sb.toString() : "";
    }

    public static void appendLog(String accountId, String line) {
        StringBuilder sb = accountLogs.computeIfAbsent(accountId, k -> new StringBuilder());
        sb.append(line).append("\n");
        if (logCallback != null) {
            logCallback.onNewLog(accountId, line);
        }
    }

    public static void updateStats(String accountId, String level, String respect, String rob, String escape) {
        if (logCallback != null) {
            logCallback.onStatsUpdated(accountId, level, respect, rob, escape);
        }
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
