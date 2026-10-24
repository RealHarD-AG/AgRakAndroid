package ir.arsacia.agrak;

import android.content.res.ColorStateList;
import android.graphics.Color;
import android.os.Bundle;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.ImageButton;
import android.widget.ScrollView;
import android.widget.TextView;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.content.ContextCompat;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

public class ChatActivity extends AppCompatActivity {

    private String accountId;
    private String accountNick;
    private TextView tvConsoleLogs;
    private ScrollView scrollConsole;
    private EditText etChatMessage;

    private Button btnTabChat;
    private Button btnTabLogs;
    private CheckBox cbFilterSmsAds;

    private boolean isChatOnlyMode = true;
    private boolean filterSmsAds = true;

    private final BotService.LogCallback chatLogCallback = new BotService.LogCallback() {
        @Override
        public void onNewLog(String accId, String line) {
            if (accountId != null && accountId.equals(accId)) {
                if (!isChatOnlyMode) {
                    runOnUiThread(() -> {
                        tvConsoleLogs.append(line + "\n");
                        scrollToBottom();
                    });
                }
            }
        }

        @Override
        public void onNewChat(String accId, String line) {
            if (accountId != null && accountId.equals(accId)) {
                if (isChatOnlyMode) {
                    if (filterSmsAds && BotService.isSmsAd(line)) {
                        return;
                    }
                    runOnUiThread(() -> {
                        CharSequence current = tvConsoleLogs.getText();
                        if (current != null && current.toString().startsWith("💬 در حال انتظار")) {
                            tvConsoleLogs.setText("");
                        }
                        tvConsoleLogs.append(line + "\n");
                        scrollToBottom();
                    });
                }
            }
        }

        @Override
        public void onStatsUpdated(String accId, String level, String respect, String rob, String escape) {}
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_chat);

        accountId = getIntent().getStringExtra("account_id");
        accountNick = getIntent().getStringExtra("account_nick");

        TextView tvName = findViewById(R.id.tvChatAccountName);
        tvConsoleLogs = findViewById(R.id.tvConsoleLogs);
        scrollConsole = findViewById(R.id.scrollConsole);
        etChatMessage = findViewById(R.id.etChatMessage);
        ImageButton btnBack = findViewById(R.id.btnBack);
        Button btnSend = findViewById(R.id.btnSendMessage);

        btnTabChat = findViewById(R.id.btnTabChat);
        btnTabLogs = findViewById(R.id.btnTabLogs);
        cbFilterSmsAds = findViewById(R.id.cbFilterSmsAds);

        Button btnReconnect = findViewById(R.id.btnQuickReconnect);
        Button btnStats = findViewById(R.id.btnQuickStats);
        Button btnSleep = findViewById(R.id.btnQuickSleep);

        tvName.setText("👤 " + (accountNick != null ? accountNick : "اکانت"));
        btnBack.setOnClickListener(v -> finish());

        // Tab selection logic
        btnTabChat.setOnClickListener(v -> {
            isChatOnlyMode = true;
            updateTabStyles();
            refreshConsoleDisplay();
        });

        btnTabLogs.setOnClickListener(v -> {
            isChatOnlyMode = false;
            updateTabStyles();
            refreshConsoleDisplay();
        });

        cbFilterSmsAds.setOnCheckedChangeListener((buttonView, isChecked) -> {
            filterSmsAds = isChecked;
            if (isChatOnlyMode) {
                refreshConsoleDisplay();
            }
        });

        // Initialize view
        updateTabStyles();
        refreshConsoleDisplay();

        // Send chat message
        btnSend.setOnClickListener(v -> {
            String msg = etChatMessage.getText().toString().trim();
            if (!msg.isEmpty()) {
                sendUserCommand(msg);
                etChatMessage.setText("");
            }
        });

        // Quick Command Shortcuts
        btnReconnect.setOnClickListener(v -> sendUserCommand("!reconnect"));
        if (btnStats != null) {
            btnStats.setOnClickListener(v -> sendUserCommand("/stats"));
        }
        btnSleep.setOnClickListener(v -> sendUserCommand("/sleep"));

        BotService.addLogCallback(chatLogCallback);
    }

    private void updateTabStyles() {
        if (isChatOnlyMode) {
            btnTabChat.setBackgroundTintList(ColorStateList.valueOf(ContextCompat.getColor(this, R.color.primary_red)));
            btnTabChat.setTextColor(Color.WHITE);
            btnTabLogs.setBackgroundTintList(ColorStateList.valueOf(Color.parseColor("#242831")));
            btnTabLogs.setTextColor(Color.parseColor("#B0B0B0"));
            cbFilterSmsAds.setEnabled(true);
            cbFilterSmsAds.setAlpha(1.0f);
        } else {
            btnTabChat.setBackgroundTintList(ColorStateList.valueOf(Color.parseColor("#242831")));
            btnTabChat.setTextColor(Color.parseColor("#B0B0B0"));
            btnTabLogs.setBackgroundTintList(ColorStateList.valueOf(ContextCompat.getColor(this, R.color.primary_red)));
            btnTabLogs.setTextColor(Color.WHITE);
            cbFilterSmsAds.setEnabled(false);
            cbFilterSmsAds.setAlpha(0.4f);
        }
    }

    private void refreshConsoleDisplay() {
        if (accountId == null) return;
        if (isChatOnlyMode) {
            String chat = BotService.getChatForAccount(accountId);
            if (chat.trim().isEmpty()) {
                tvConsoleLogs.setText("💬 در حال انتظار برای دریافت چت و پیام‌های سرور آرساکیا...\n(پیام‌های سیستمی و زائد برنامه فیلتر شدند)\n");
            } else {
                if (filterSmsAds) {
                    StringBuilder filtered = new StringBuilder();
                    for (String line : chat.split("\n")) {
                        if (!BotService.isSmsAd(line)) {
                            filtered.append(line).append("\n");
                        }
                    }
                    tvConsoleLogs.setText(filtered.length() > 0 ? filtered.toString() : "💬 در حال انتظار برای دریافت چت...\n");
                } else {
                    tvConsoleLogs.setText(chat);
                }
            }
        } else {
            String logs = BotService.getLogsForAccount(accountId);
            tvConsoleLogs.setText(!logs.trim().isEmpty() ? logs : "📜 لاگی برای این اکانت ثبت نشده است.\n");
        }
        scrollToBottom();
    }

    @Override
    protected void onDestroy() {
        BotService.removeLogCallback(chatLogCallback);
        super.onDestroy();
    }

    private void sendUserCommand(String cmd) {
        if (accountNick != null && !cmd.isEmpty()) {
            BotService.sendLocalCommand(accountNick, cmd, true);

            String timeStr = new SimpleDateFormat("HH:mm:ss", Locale.getDefault()).format(new Date());
            String userLine = "[" + timeStr + "] 📤 من: " + cmd;
            BotService.appendChat(accountId, userLine);

            if (isChatOnlyMode) {
                CharSequence current = tvConsoleLogs.getText();
                if (current != null && current.toString().startsWith("💬 در حال انتظار")) {
                    tvConsoleLogs.setText("");
                }
                tvConsoleLogs.append(userLine + "\n");
                scrollToBottom();
            }
        }
    }

    private void scrollToBottom() {
        scrollConsole.post(() -> scrollConsole.fullScroll(ScrollView.FOCUS_DOWN));
    }
}
