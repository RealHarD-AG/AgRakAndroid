package ir.arsacia.agrak;

import android.os.Bundle;
import android.text.SpannableStringBuilder;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.ImageButton;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;
import androidx.appcompat.app.AppCompatActivity;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

public class ChatActivity extends AppCompatActivity {

    private String accountId;
    private String accountNick;
    private TextView tvConsoleLogs, tvChatStatus;
    private ScrollView scrollConsole;
    private EditText etChatMessage;

    private final android.os.Handler statusHandler = new android.os.Handler(android.os.Looper.getMainLooper());
    private final Runnable statusRunnable = new Runnable() {
        @Override
        public void run() {
            updateChatStatus();
            statusHandler.postDelayed(this, 1000);
        }
    };

    private final BotService.LogCallback chatLogCallback = new BotService.LogCallback() {
        @Override
        public void onNewLog(String accId, String line) {
            // Internal technical logs are suppressed from the chat view
        }

        @Override
        public void onNewChat(String accId, String line) {
            if (accountId != null && accountId.equals(accId)) {
                if (BotService.isStatsDumpLine(line)) {
                    return;
                }
                runOnUiThread(() -> {
                    boolean hadFocus = etChatMessage != null && etChatMessage.hasFocus();
                    int selStart = hadFocus ? etChatMessage.getSelectionStart() : -1;
                    int selEnd = hadFocus ? etChatMessage.getSelectionEnd() : -1;

                    CharSequence current = tvConsoleLogs.getText();
                    if (current != null && current.toString().contains("در حال انتظار")) {
                        tvConsoleLogs.setText("");
                    }
                    tvConsoleLogs.append(ColorSpanHelper.parseSampColors(line));
                    tvConsoleLogs.append("\n");
                    scrollToBottom();

                    if (hadFocus) {
                        etChatMessage.requestFocus();
                        if (selStart >= 0 && selEnd >= 0 && selEnd <= etChatMessage.getText().length()) {
                            etChatMessage.setSelection(selStart, selEnd);
                        }
                    }
                });
            }
        }

        @Override
        public void onStatsUpdated(String accId, String level, String respect, String rob, String escape) {
            // Stats display removed from console
        }

        @Override
        public void onConnected(String accId) {
            if (accountId != null && (accountId.equals(accId) || (accountNick != null && accountNick.equalsIgnoreCase(accId)))) {
                runOnUiThread(() -> {
                    updateChatStatus();
                });
            }
        }

        @Override
        public void onDisconnected(String accId) {
            if (accountId != null && (accountId.equals(accId) || (accountNick != null && accountNick.equalsIgnoreCase(accId)))) {
                runOnUiThread(() -> {
                    updateChatStatus();
                    if (!BotService.isBotRunning(accountNick)) {
                        BotService.clearAccountLogsAndChat(accId);
                    }
                    refreshConsoleDisplay();
                });
            }
        }

        @Override
        public void onAccountCleared(String accId) {
            if (accountId != null && (accountId.equals(accId) || (accountNick != null && accountNick.equalsIgnoreCase(accId)))) {
                runOnUiThread(() -> refreshConsoleDisplay());
            }
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_chat);

        accountId = getIntent().getStringExtra("account_id");
        accountNick = getIntent().getStringExtra("account_nick");

        TextView tvName = findViewById(R.id.tvChatAccountName);
        tvChatStatus = findViewById(R.id.tvChatStatus);
        tvConsoleLogs = findViewById(R.id.tvConsoleLogs);
        scrollConsole = findViewById(R.id.scrollConsole);
        etChatMessage = findViewById(R.id.etChatMessage);
        ImageButton btnBack = findViewById(R.id.btnBack);
        Button btnSend = findViewById(R.id.btnSendMessage);

        Button btnReconnect = findViewById(R.id.btnQuickReconnect);

        tvName.setText(accountNick != null ? accountNick : "اکانت");
        updateChatStatus();
        btnBack.setOnClickListener(v -> finish());

        // Initialize view
        refreshConsoleDisplay();

        // Send chat message
        btnSend.setOnClickListener(v -> {
            String msg = etChatMessage.getText().toString().trim();
            if (!msg.isEmpty()) {
                sendUserCommand(msg);
                etChatMessage.setText("");
            }
        });

        // Quick Reconnect Shortcut only
        btnReconnect.setOnClickListener(v -> sendUserCommand("!reconnect"));

        BotService.addLogCallback(chatLogCallback);
    }

    @Override
    protected void onResume() {
        super.onResume();
        updateChatStatus();
        refreshConsoleDisplay();
        statusHandler.post(statusRunnable);
    }

    @Override
    protected void onPause() {
        super.onPause();
        statusHandler.removeCallbacks(statusRunnable);
    }

    private void updateChatStatus() {
        if (tvChatStatus == null) return;
        boolean running = BotService.isBotRunning(accountNick);
        boolean connected = running && BotService.isAccountConnected(accountNick);
        if (connected) {
            tvChatStatus.setText("🟢 آنلاین در آرساکیا");
            tvChatStatus.setTextColor(getResources().getColor(R.color.status_green));
        } else if (running) {
            tvChatStatus.setText("🟡 در حال اتصال به سرور...");
            tvChatStatus.setTextColor(getResources().getColor(R.color.status_yellow));
        } else {
            tvChatStatus.setText("🔴 آفلاین");
            tvChatStatus.setTextColor(getResources().getColor(R.color.status_red));
        }
    }

    private void refreshConsoleDisplay() {
        if (accountId == null) return;
        boolean running = BotService.isBotRunning(accountNick);
        if (!running) {
            tvConsoleLogs.setText(ColorSpanHelper.parseSampColors("{7E8B9B}💬 اکانت آفلاین است. برای مشاهده چت، ربات را روشن کنید.\n"), TextView.BufferType.SPANNABLE);
            return;
        }
        String chat = BotService.getChatForAccount(accountId);
        if (chat == null || chat.trim().isEmpty()) {
            tvConsoleLogs.setText(ColorSpanHelper.parseSampColors("{7E8B9B}💬 در حال انتظار برای دریافت چت رسمی سرور...\n"), TextView.BufferType.SPANNABLE);
        } else {
            SpannableStringBuilder ssb = new SpannableStringBuilder();
            for (String line : chat.split("\n")) {
                if (line.trim().isEmpty()) continue;
                if (BotService.isStatsDumpLine(line)) {
                    continue;
                }
                ssb.append(ColorSpanHelper.parseSampColors(line)).append("\n");
            }
            tvConsoleLogs.setText(ssb.length() > 0 ? ssb : ColorSpanHelper.parseSampColors("{7E8B9B}💬 هیچ پیامی دریافت نشده است.\n"), TextView.BufferType.SPANNABLE);
        }
        scrollToBottom();
    }

    @Override
    protected void onDestroy() {
        statusHandler.removeCallbacks(statusRunnable);
        BotService.removeLogCallback(chatLogCallback);
        super.onDestroy();
    }

    private void sendUserCommand(String cmd) {
        if (accountNick != null && !cmd.isEmpty()) {
            if (!BotService.isBotRunning(accountNick)) {
                Toast.makeText(this, "اکانت آفلاین است و امکان ارسال دستور وجود ندارد", Toast.LENGTH_SHORT).show();
                return;
            }
            BotService.sendLocalCommand(accountNick, cmd, true);

            String timeStr = new SimpleDateFormat("HH:mm:ss", Locale.getDefault()).format(new Date());
            String userLine = "[" + timeStr + "] {00E5FF}📤 من: {FFFFFF}" + cmd;
            BotService.appendChat(accountId, userLine);

            CharSequence current = tvConsoleLogs.getText();
            if (current != null && current.toString().contains("در حال انتظار")) {
                tvConsoleLogs.setText("");
            }
            tvConsoleLogs.append(ColorSpanHelper.parseSampColors(userLine));
            tvConsoleLogs.append("\n");
            scrollToBottom();
        }
    }

    private void scrollToBottom() {
        if (scrollConsole != null && tvConsoleLogs != null) {
            scrollConsole.post(() -> {
                int scrollY = tvConsoleLogs.getBottom() - scrollConsole.getHeight();
                if (scrollY > 0) {
                    scrollConsole.smoothScrollTo(0, scrollY);
                }
            });
        }
    }
}
