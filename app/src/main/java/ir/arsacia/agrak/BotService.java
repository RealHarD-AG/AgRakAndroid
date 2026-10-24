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
import android.util.Log;
import androidx.annotation.Nullable;
import androidx.core.app.NotificationCompat;
import java.io.BufferedReader;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.CopyOnWriteArrayList;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.TimeUnit;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class BotService extends Service {
    private static final String TAG = "AGRak_BotService";
    public static final String CHANNEL_ID = "AGRak_Channel";
    public static final int NOTIFICATION_ID = 1001;

    public interface LogCallback {
        void onNewLog(String accountId, String line);
        default void onNewChat(String accountId, String line) {}
        void onStatsUpdated(String accountId, String level, String respect, String rob, String escape);
    }

    private static final List<LogCallback> logCallbacks = new CopyOnWriteArrayList<>();
    private static final Map<String, StringBuilder> accountLogs = Collections.synchronizedMap(new HashMap<>());
    private static final Map<String, StringBuilder> accountChat = Collections.synchronizedMap(new HashMap<>());
    private static final Map<String, String[]> cachedStats = Collections.synchronizedMap(new HashMap<>());

    // Running local processes mapped by lowercase nickname
    private static final Map<String, Process> activeProcesses = new ConcurrentHashMap<>();
    private static final Map<String, Thread> readerThreads = new ConcurrentHashMap<>();
    private static final Map<String, String> nickToAccountId = new ConcurrentHashMap<>();

    private PowerManager.WakeLock wakeLock;

    private static final Pattern PATTERN_LEVEL_RESPECT = Pattern.compile("Level\\s*\\[(\\d+)\\]\\s*Respect\\s*\\[([0-9/]+)\\]", Pattern.CASE_INSENSITIVE);
    private static final Pattern PATTERN_ROB_ESCAPE = Pattern.compile("Rob points\\s*\\[(\\d+)\\]\\s*Escape points\\s*\\[(\\d+)\\]", Pattern.CASE_INSENSITIVE);
    private static final Pattern PATTERN_LEVEL = Pattern.compile("Level\\s*[:\\[\\s]*(\\d+)", Pattern.CASE_INSENSITIVE);
    private static final Pattern PATTERN_RESPECT = Pattern.compile("Respect\\s*[:\\[\\s]*([0-9/]+)", Pattern.CASE_INSENSITIVE);
    private static final Pattern PATTERN_ROB = Pattern.compile("Rob\\s*(?:points)?\\s*[:\\[\\s]*(\\d+)", Pattern.CASE_INSENSITIVE);
    private static final Pattern PATTERN_ESCAPE = Pattern.compile("Escape\\s*(?:points)?\\s*[:\\[\\s]*(\\d+)", Pattern.CASE_INSENSITIVE);
    private static final ScheduledExecutorService scheduler = Executors.newSingleThreadScheduledExecutor();

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

    public static String getLogsForAccount(String accountId) {
        if (accountId == null) return "";
        StringBuilder sb = accountLogs.get(accountId);
        return sb != null ? sb.toString() : "";
    }

    public static String getChatForAccount(String accountId) {
        if (accountId == null) return "";
        StringBuilder sb = accountChat.get(accountId);
        return sb != null ? sb.toString() : "";
    }

    public static void appendChat(String accountId, String line) {
        if (accountId == null || line == null) return;
        StringBuilder sb = accountChat.computeIfAbsent(accountId, k -> new StringBuilder());
        sb.append(line).append("\n");
        if (sb.length() > 200000) {
            sb.delete(0, 50000);
        }
    }

    public static boolean isServerChatLine(String line) {
        if (line == null) return false;
        String trimmed = line.trim();
        if (trimmed.isEmpty()) return false;

        // User outgoing command or message
        if (trimmed.contains("📤 من:") || (trimmed.startsWith("> ") && !trimmed.startsWith("> /stats"))) {
            return true;
        }

        // Key connection and state events to show in chat console
        if (trimmed.contains("Connecting to ") ||
                trimmed.contains("Connected to ") ||
                trimmed.contains("Connected. Joining the game") ||
                trimmed.contains("Account confirmed in Sleep Mode") ||
                trimmed.contains("اسلیپ فعال است") ||
                trimmed.contains("You have been spawned") ||
                trimmed.contains("You are banned") ||
                trimmed.contains("You were kicked") ||
                trimmed.contains("Connection was closed") ||
                trimmed.contains("Server closed the connection")) {
            return true;
        }

        // Filter out app and engine technical messages
        if (trimmed.startsWith("[AGRak]") ||
                trimmed.contains("[AutoLogin]") ||
                trimmed.contains("[AutoStats]") ||
                trimmed.contains("[RakSAMP]") ||
                trimmed.contains("[LUA]") ||
                trimmed.contains("NET]") ||
                trimmed.contains("HandleRPCPacket") ||
                trimmed.contains("[STEP]") ||
                trimmed.contains("[لاگ موتور]") ||
                trimmed.contains("=================================") ||
                trimmed.contains("Loading scripts") ||
                trimmed.contains("autologin.lua") ||
                trimmed.contains("========== DIALOG") ||
                trimmed.contains("Type response for") ||
                trimmed.contains("Choice:") ||
                trimmed.startsWith("[1] ") ||
                trimmed.contains("[GAMETEXT]") ||
                trimmed.contains("[AUDIO_STREAM]") ||
                trimmed.contains("[MENU") ||
                trimmed.contains("[SELECTABLE-TEXTDRAW]") ||
                trimmed.contains("[DESTROYPICKUP]") ||
                trimmed.contains("[CP]") ||
                trimmed.startsWith("> /stats")) {
            return false;
        }

        // Filter periodic stats dumps from the chat tab (they update the top stats cards)
        if (trimmed.contains("*** [") && trimmed.contains(") ***")) return false;
        if (trimmed.contains("Account: Sex [") ||
                trimmed.contains("Activity: Level [") ||
                trimmed.contains("Crimes: Status [") ||
                trimmed.contains("Punishes: Mute [") ||
                trimmed.contains("Resources: Gold [") ||
                trimmed.contains("Inventory: Drugs [") ||
                trimmed.contains("Faction: Faction [") ||
                trimmed.contains("_______________________________________________")) {
            return false;
        }

        // Must be SA:MP player chat or server client message
        if (trimmed.contains("[CHAT]") || trimmed.contains("[CMSG]")) {
            return true;
        }

        return false;
    }

    public static boolean isSmsAd(String line) {
        if (line == null) return false;
        String lower = line.toLowerCase();
        return lower.contains("/sms") ||
                lower.contains("tablighate") ||
                lower.contains("tablighat") ||
                lower.contains("paziraye tablighat") ||
                lower.contains("besparesh dast ma") ||
                lower.contains("nr [") ||
                lower.contains("nr:") ||
                lower.contains("[sms]");
    }

    public static String formatIfServerChat(String line) {
        if (!isServerChatLine(line)) return null;

        String formatted = line.trim();

        // Extract timestamp [HH:MM:SS.mmm] or [HH:MM:SS]
        String timePrefix = "";
        int timeEnd = formatted.indexOf("] ");
        if (formatted.startsWith("[") && timeEnd > 0 && timeEnd <= 15) {
            String rawTime = formatted.substring(1, timeEnd);
            int dotIdx = rawTime.indexOf('.');
            if (dotIdx > 0) {
                rawTime = rawTime.substring(0, dotIdx);
            }
            timePrefix = "[" + rawTime + "] ";
            formatted = formatted.substring(timeEnd + 2).trim();
        }

        if (formatted.startsWith("[CHAT]")) {
            String chatContent = formatted.substring(6).trim();
            return timePrefix + "[چت] " + chatContent;
        } else if (formatted.startsWith("[CMSG]")) {
            String cmsgContent = formatted.substring(6).trim();
            return timePrefix + cmsgContent;
        } else if (formatted.contains("Account confirmed in Sleep Mode") || formatted.contains("اسلیپ فعال است")) {
            return timePrefix + "💤 [اسلیپ] اکانت با موفقیت در حالت اسلیپ تایید شد.";
        } else if (formatted.startsWith("Connecting to")) {
            return timePrefix + "🔄 [اتصال] در حال اتصال به سرور آرساکیا گیم...";
        } else if (formatted.startsWith("Connected to") || formatted.startsWith("Connected.")) {
            return timePrefix + "🟢 [اتصال] اتصال موفق به سرور آرساکیا گیم";
        } else if (formatted.contains("spawned")) {
            return timePrefix + "🎮 [بازی] ورود به بازی انجام شد.";
        } else if (formatted.contains("kicked") || formatted.contains("closed the connection") || formatted.contains("banned")) {
            return timePrefix + "🔴 [قطع] ارتباط با سرور قطع شد.";
        } else {
            return timePrefix + formatted;
        }
    }

    public static void appendLog(String accountId, String line) {
        if (accountId == null || line == null) return;
        StringBuilder sb = accountLogs.computeIfAbsent(accountId, k -> new StringBuilder());
        sb.append(line).append("\n");
        if (sb.length() > 200000) {
            sb.delete(0, 50000);
        }

        parseLineForStats(accountId, line);

        String cleanChat = formatIfServerChat(line);
        if (cleanChat != null) {
            appendChat(accountId, cleanChat);
        }

        for (LogCallback cb : logCallbacks) {
            try {
                cb.onNewLog(accountId, line);
                if (cleanChat != null) {
                    cb.onNewChat(accountId, cleanChat);
                }
            } catch (Exception ignored) {}
        }
    }

    public static void parseLineForStats(String accountId, String line) {
        if (accountId == null || line == null) return;
        String[] stats = cachedStats.computeIfAbsent(accountId, k -> new String[]{"--", "--", "--", "--"});
        boolean changed = false;

        Matcher mLR = PATTERN_LEVEL_RESPECT.matcher(line);
        if (mLR.find()) {
            stats[0] = mLR.group(1);
            stats[1] = mLR.group(2);
            changed = true;
        } else {
            Matcher mRes = PATTERN_RESPECT.matcher(line);
            Matcher mLvl = PATTERN_LEVEL.matcher(line);
            if (mRes.find()) {
                stats[1] = mRes.group(1);
                changed = true;
                if (mLvl.find()) {
                    stats[0] = mLvl.group(1);
                }
            } else if (line.contains("Activity:") && mLvl.find()) {
                stats[0] = mLvl.group(1);
                changed = true;
            }
        }

        Matcher mRE = PATTERN_ROB_ESCAPE.matcher(line);
        if (mRE.find()) {
            stats[2] = mRE.group(1);
            stats[3] = mRE.group(2);
            changed = true;
        } else if (line.contains("Resources:") || line.toLowerCase().contains("rob points") || line.toLowerCase().contains("escape points")) {
            Matcher mRob = PATTERN_ROB.matcher(line);
            Matcher mEsc = PATTERN_ESCAPE.matcher(line);
            if (mRob.find()) {
                stats[2] = mRob.group(1);
                changed = true;
            }
            if (mEsc.find()) {
                stats[3] = mEsc.group(1);
                changed = true;
            }
        }

        if (changed) {
            for (Map.Entry<String, String> entry : nickToAccountId.entrySet()) {
                if (accountId.equals(entry.getValue())) {
                    cachedStats.put(entry.getKey(), stats);
                    break;
                }
            }

            for (LogCallback cb : logCallbacks) {
                try {
                    cb.onStatsUpdated(accountId, stats[0], stats[1], stats[2], stats[3]);
                } catch (Exception ignored) {}
            }
        }
    }

    public static String[] getCachedStats(String keyOrId) {
        if (keyOrId == null) return null;
        String[] s = cachedStats.get(keyOrId);
        if (s == null) {
            String accId = nickToAccountId.get(keyOrId.toLowerCase());
            if (accId != null) {
                s = cachedStats.get(accId);
            }
        }
        return s;
    }

    public static void scheduleCommand(String nickname, String cmd, int delaySeconds) {
        if (nickname == null || cmd == null) return;
        try {
            scheduler.schedule(() -> sendLocalCommand(nickname, cmd, false), delaySeconds, TimeUnit.SECONDS);
        } catch (Exception ignored) {}
    }

    public static boolean isBotRunning(String nickname) {
        if (nickname == null) return false;
        Process proc = activeProcesses.get(nickname.toLowerCase());
        if (proc != null) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                return proc.isAlive();
            }
            try {
                proc.exitValue();
                return false;
            } catch (IllegalThreadStateException e) {
                return true;
            }
        }
        return false;
    }

    public static int getActiveBotsCount() {
        int count = 0;
        for (Map.Entry<String, Process> entry : activeProcesses.entrySet()) {
            if (isBotRunning(entry.getKey())) {
                count++;
            }
        }
        return count;
    }

    public static synchronized void startLocalBot(Context context, AccountModel acc) {
        if (acc == null || acc.getNickname() == null) return;
        String nick = acc.getNickname().trim();
        String key = nick.toLowerCase();
        nickToAccountId.put(key, acc.getId());

        // Stop existing process if any
        stopLocalBot(nick);

        appendLog(acc.getId(), "[AGRak] آماده‌سازی محیط اجرای داخلی ربات برای " + nick + "...");

        try {
            File botDir = new File(context.getFilesDir(), "bots/" + key);
            botDir.mkdirs();
            File scriptsDir = new File(botDir, "scripts");
            scriptsDir.mkdirs();
            File luaDir = new File(botDir, "lua");
            luaDir.mkdirs();

            // 1. Copy addon.lua to both lua/ and scripts/
            copyAssetToFile(context, "lua/addon.lua", new File(luaDir, "addon.lua"));
            copyAssetToFile(context, "lua/addon.lua", new File(scriptsDir, "addon.lua"));

            // 2. Prepare autologin.lua and write account_config.lua
            writeAccountConfig(scriptsDir, acc);
            writeAccountConfig(botDir, acc);
            prepareAutologinScript(context, acc, new File(scriptsDir, "autologin.lua"));

            // 3. Write RakSAMPClient.xml (server join password must be empty for ArSaCiA)
            File xmlFile = new File(botDir, "RakSAMPClient.xml");
            writeRakSAMPConfig(xmlFile, nick);

            // 4. Resolve binary path from nativeLibraryDir
            String nativeDir = context.getApplicationInfo().nativeLibraryDir;
            File binary = new File(nativeDir, "libraksamp.so");
            if (!binary.exists()) {
                appendLog(acc.getId(), "[AGRak] ❌ فایل بومی libraksamp.so در مسیر " + nativeDir + " یافت نشد. لطفاً برنامه را مجدداً نصب فرمایید.");
                return;
            }

            appendLog(acc.getId(), "[AGRak] موتور بومی راک‌سمپ: " + binary.getAbsolutePath());
            appendLog(acc.getId(), "[AGRak] در حال اجرای موتور راک‌سمپ با اینترنت گوشی...");

            ProcessBuilder pb = new ProcessBuilder(
                    binary.getAbsolutePath(),
                    "--workdir", botDir.getAbsolutePath(),
                    "--config", xmlFile.getAbsolutePath()
            );
            pb.directory(botDir);
            pb.redirectErrorStream(true);

            Map<String, String> env = pb.environment();
            env.put("LD_LIBRARY_PATH", nativeDir + ":" + botDir.getAbsolutePath());

            Process process = pb.start();
            activeProcesses.put(key, process);

            // Read output stream
            Thread readerThread = new Thread(() -> {
                try (BufferedReader reader = new BufferedReader(new InputStreamReader(process.getInputStream(), StandardCharsets.UTF_8))) {
                    String line;
                    while ((line = reader.readLine()) != null) {
                        appendLog(acc.getId(), line);

                        String lower = line.toLowerCase();

                        // Auto-response in Java as backup if console prompts for password or spawn
                        if (acc.isAutoLogin()) {
                            if ((lower.contains("type response for [login]") || lower.contains("type response for"))
                                    && acc.getPassword() != null && !acc.getPassword().isEmpty()) {
                                try {
                                    Thread.sleep(300);
                                    OutputStream os = process.getOutputStream();
                                    os.write((acc.getPassword() + "\n").getBytes(StandardCharsets.UTF_8));
                                    os.flush();
                                    appendLog(acc.getId(), "[AGRak] 🔑 رمز عبور به صورت خودکار به دیالوگ لاگین ارسال شد.");

                                    // Schedule stats query 15s after password (when in-game)
                                    scheduleCommand(key, "/stats", 15);
                                } catch (Exception ex) {
                                    Log.e(TAG, "Error auto-submitting password", ex);
                                }
                            } else if (lower.contains("write !spawn into the console")) {
                                try {
                                    Thread.sleep(600);
                                    OutputStream os = process.getOutputStream();
                                    os.write("!spawn\n".getBytes(StandardCharsets.UTF_8));
                                    os.flush();
                                    appendLog(acc.getId(), "[AGRak] 🚀 دستور !spawn خودکار ارسال شد.");
                                } catch (Exception ex) {
                                    Log.e(TAG, "Error auto-spawning", ex);
                                }
                            }
                        }

                        // Auto-trigger stats on login or sleep confirmation
                        if (lower.contains("welcome ") || lower.contains("put to sleep") || lower.contains("confirmed in sleep mode") || lower.contains("auto-confirming")) {
                            scheduleCommand(key, "/stats", 3);
                        } else if (lower.contains("can't execute commands when a dialog is shown") || lower.contains("dialog is shown up")) {
                            // Retry stats if server blocked it due to an open dialog
                            scheduleCommand(key, "/stats", 4);
                        }
                    }
                } catch (Exception e) {
                    appendLog(acc.getId(), "[AGRak] خطای خواندن جریان لاگ: " + e.getMessage());
                } finally {
                    try {
                        int exitCode = process.waitFor();
                        appendLog(acc.getId(), "[AGRak] پروسه راک‌سمپ با کد خروجی " + exitCode + " خاتمه یافت.");
                        if (exitCode != 0) {
                            File crashLog = new File(botDir, "RakSAMPClient.log");
                            if (crashLog.exists() && crashLog.length() > 0) {
                                try (BufferedReader cr = new BufferedReader(new FileReader(crashLog))) {
                                    String cl;
                                    List<String> lastLines = new ArrayList<>();
                                    while ((cl = cr.readLine()) != null) {
                                        lastLines.add(cl);
                                        if (lastLines.size() > 35) lastLines.remove(0);
                                    }
                                    for (String l : lastLines) {
                                        appendLog(acc.getId(), "[لاگ موتور] " + l);
                                    }
                                } catch (Exception ignored) {}
                            }
                        }
                    } catch (Exception ignored) {}
                    activeProcesses.remove(key);
                    appendLog(acc.getId(), "[AGRak] وضعیت اکانت: قطع اتصال (آفلاین).");
                }
            });
            readerThread.setDaemon(true);
            readerThread.start();
            readerThreads.put(key, readerThread);

        } catch (Exception e) {
            Log.e(TAG, "Error starting local bot", e);
            appendLog(acc.getId(), "[AGRak] ❌ خطا در راه‌اندازی ربات: " + e.getClass().getSimpleName() + " - " + e.getMessage());
        }
    }

    public static synchronized void stopLocalBot(String nickname) {
        if (nickname == null) return;
        String key = nickname.toLowerCase();
        Process proc = activeProcesses.remove(key);
        if (proc != null) {
            try {
                proc.destroy();
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                    proc.destroyForcibly();
                }
            } catch (Exception ignored) {}
        }
        Thread t = readerThreads.remove(key);
        if (t != null && t.isAlive()) {
            t.interrupt();
        }
        String accId = nickToAccountId.get(key);
        if (accId != null) {
            appendLog(accId, "[AGRak] ربات با موفقیت توسط کاربر متوقف شد.");
        }
    }

    public static void sendLocalCommand(String nickname, String cmd) {
        sendLocalCommand(nickname, cmd, false);
    }

    public static void sendLocalCommand(String nickname, String cmd, boolean logToConsole) {
        if (nickname == null || cmd == null) return;
        String key = nickname.toLowerCase();
        Process proc = activeProcesses.get(key);
        if (proc != null) {
            try {
                OutputStream os = proc.getOutputStream();
                os.write((cmd + "\n").getBytes(StandardCharsets.UTF_8));
                os.flush();
                if (logToConsole) {
                    String accId = nickToAccountId.get(key);
                    if (accId != null) {
                        appendLog(accId, "> " + cmd);
                    }
                }
            } catch (Exception e) {
                Log.e(TAG, "Error sending command", e);
            }
        }
    }

    private static void writeRakSAMPConfig(File xmlFile, String nickname) {
        String content = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n" +
                "<RakSAMPClient console=\"1\" dialogs=\"console\" runmode=\"3\" autorun=\"0\" find=\"1\" select_classid=\"0\" manual_spawn=\"1\"\n" +
                "               print_timestamps=\"1\" log_level=\"1\" chatcolor_rgb=\"0 0 130\" clientmsg_rgb=\"0 130 0\" cpalert_rgb=\"170 0 0\"\n" +
                "               updatestats=\"1\" minfps=\"20\" maxfps=\"90\" clientversion=\"0.3.7\">\n" +
                "    <server nickname=\"" + nickname + "\" password=\"\">185.112.33.99:7777</server>\n" +
                "    <intervals spam=\"150\" fakekill=\"35\" lag=\"20\" joinflood=\"50\" chatflood=\"20\" classflood=\"30\" bulletflood=\"25\" />\n" +
                "    <log level=\"1\" objects=\"0\" pickups=\"0\" textlabels=\"0\" textdraws=\"0\" />\n" +
                "    <sendrates force=\"0\" onfoot=\"40\" incar=\"40\" firing=\"40\" multiplier=\"1\" />\n" +
                "    <scripts>autologin</scripts>\n" +
                "    <rates sleep=\"30\" connect=\"6000\" reconnect=\"3000\" network=\"0\" lua=\"50\" spectate=\"30\" />\n" +
                "    <normal_pos position=\"325.35 2512.09 16.56\" rotation=\"0.0\" force=\"0\" />\n" +
                "</RakSAMPClient>\n";

        try (FileOutputStream fos = new FileOutputStream(xmlFile)) {
            fos.write(content.getBytes(StandardCharsets.UTF_8));
        } catch (Exception e) {
            Log.e(TAG, "Failed to write RakSAMPClient.xml", e);
        }
    }

    private static void writeAccountConfig(File scriptsDir, AccountModel acc) {
        File configFile = new File(scriptsDir, "account_config.lua");
        String password = (acc != null && acc.getPassword() != null) ? acc.getPassword() : "";
        boolean autoLogin = (acc != null && acc.isAutoLogin());
        String escapedPass = password.replace("\\", "\\\\").replace("\"", "\\\"");

        String content = "-- Auto-generated account config for AGRak\n" +
                "PASSWORD = \"" + escapedPass + "\"\n" +
                "AUTO_LOGIN_ENABLED = " + (autoLogin && !password.isEmpty() ? "true" : "false") + "\n";

        try (FileOutputStream fos = new FileOutputStream(configFile)) {
            fos.write(content.getBytes(StandardCharsets.UTF_8));
        } catch (Exception e) {
            Log.e(TAG, "Failed to write account_config.lua", e);
        }
    }

    private static void prepareAutologinScript(Context context, AccountModel acc, File destFile) {
        try {
            ByteArrayOutputStream baos = new ByteArrayOutputStream();
            try (InputStream is = context.getAssets().open("scripts/autologin.lua")) {
                byte[] buf = new byte[4096];
                int r;
                while ((r = is.read(buf)) != -1) {
                    baos.write(buf, 0, r);
                }
            }
            String scriptContent = baos.toString("UTF-8");
            String password = (acc != null && acc.getPassword() != null) ? acc.getPassword() : "";
            boolean autoLogin = (acc != null && acc.isAutoLogin());

            if (autoLogin && !password.isEmpty()) {
                String escapedPass = password.replace("\\", "\\\\").replace("\"", "\\\"");
                scriptContent = scriptContent.replaceFirst("local PASSWORD = [^\r\n]+", java.util.regex.Matcher.quoteReplacement("local PASSWORD = \"" + escapedPass + "\""));
                scriptContent = scriptContent.replaceFirst("local AUTO_LOGIN_ENABLED = [^\r\n]+", "local AUTO_LOGIN_ENABLED = true");
            } else {
                scriptContent = scriptContent.replaceFirst("local PASSWORD = [^\r\n]+", "local PASSWORD = \"\"");
                scriptContent = scriptContent.replaceFirst("local AUTO_LOGIN_ENABLED = [^\r\n]+", "local AUTO_LOGIN_ENABLED = false");
            }

            try (FileOutputStream fos = new FileOutputStream(destFile)) {
                fos.write(scriptContent.getBytes(StandardCharsets.UTF_8));
            }
        } catch (Exception e) {
            Log.e(TAG, "Failed to prepare autologin script", e);
        }
    }

    private static void copyAssetToFile(Context context, String assetPath, File destFile) {
        try (InputStream is = context.getAssets().open(assetPath);
             FileOutputStream fos = new FileOutputStream(destFile)) {
            byte[] buf = new byte[4096];
            int len;
            while ((len = is.read(buf)) > 0) {
                fos.write(buf, 0, len);
            }
        } catch (Exception e) {
            Log.e(TAG, "Failed to copy asset " + assetPath, e);
        }
    }

    private static void copyFile(File src, File dst) {
        try (InputStream in = new java.io.FileInputStream(src);
             OutputStream out = new FileOutputStream(dst)) {
            byte[] buf = new byte[4096];
            int len;
            while ((len = in.read(buf)) > 0) {
                out.write(buf, 0, len);
            }
        } catch (Exception e) {
            Log.e(TAG, "Failed to copy file", e);
        }
    }

    @Override
    public void onCreate() {
        super.onCreate();
        createNotificationChannel();
        acquireWakeLock();
        startForeground(NOTIFICATION_ID, createNotification("سرویس داخلی راک‌سمپ AGRak آماده است"));

        // Periodically refresh stats every 90 seconds for all active bots (silent)
        scheduler.scheduleWithFixedDelay(() -> {
            try {
                for (String activeNick : activeProcesses.keySet()) {
                    sendLocalCommand(activeNick, "/stats", false);
                }
            } catch (Exception ignored) {}
        }, 30, 90, TimeUnit.SECONDS);
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
            channel.setDescription("اجرای مستقل ربات روی گوشی با اینترنت همراه");
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
                ? activeCount + " اکانت آنلاین با اینترنت گوشی در سرور 185.112.33.99"
                : "آماده اتصال به سرور آرساکیا (مستقل)";
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
        for (String nick : activeProcesses.keySet()) {
            stopLocalBot(nick);
        }
        super.onDestroy();
    }

    @Nullable
    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }
}