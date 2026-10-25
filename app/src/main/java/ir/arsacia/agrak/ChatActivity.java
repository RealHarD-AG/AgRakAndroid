package ir.arsacia.agrak;

import android.os.Bundle;
import android.text.SpannableStringBuilder;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.ImageButton;
import android.widget.ScrollView;
import android.widget.TextView;
import androidx.appcompat.app.AppCompatActivity;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

public class ChatActivity extends AppCompatActivity {

    private String accountId;
    private String accountNick;
    private TextView tvConsoleLogs;
    private ScrollView scrollConsole;
    private EditText etChatMessage;
    private CheckBox cbFilterSmsAds;

    private boolean filterSmsAds = true;

    private final BotService.LogCallback chatLogCallback = new BotService.LogCallback() {
        @Override
        public void onNewLog(String accId, String line) {
            // Internal technical logs are suppressed from the chat view
        }

        @Override
        public void onNewChat(String accId, String line) {
            if (accountId != null && accountId.equals(accId)) {
                if (filterSmsAds && BotService.isSmsAd(line)) {
                    return;
                }
                runOnUiThread(() -> {
                    CharSequence current = tvConsoleLogs.getText();
                    if (current != null && current.toString().contains("در حال انتظار")) {
                        tvConsoleLogs.setText("");
                    }
                    tvConsoleLogs.append(ColorSpanHelper.parseSampColors(line));
                    tvConsoleLogs.append("\n");
                    scrollToBottom();
                });
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

        cbFilterSmsAds = findViewById(R.id.cbFilterSmsAds);

        Button btnReconnect = findViewById(R.id.btnQuickReconnect);
        Button btnStats = findViewById(R.id.btnQuickStats);
        Button btnSleep = findViewById(R.id.btnQuickSleep);

        tvName.setText(accountNick != null ? accountNick : "اکانت");
        btnBack.setOnClickListener(v -> finish());

        cbFilterSmsAds.setOnCheckedChangeListener((buttonView, isChecked) -> {
            filterSmsAds = isChecked;
            refreshConsoleDisplay();
        });

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

        // Quick Command Shortcuts
        btnReconnect.setOnClickListener(v -> sendUserCommand("!reconnect"));
        if (btnStats != null) {
            btnStats.setOnClickListener(v -> sendUserCommand("/stats"));
        }
        btnSleep.setOnClickListener(v -> sendUserCommand("/sleep"));

        BotService.addLogCallback(chatLogCallback);
    }

    private void refreshConsoleDisplay() {
        if (accountId == null) return;
        String chat = BotService.getChatForAccount(accountId);
        if (chat.trim().isEmpty()) {
            tvConsoleLogs.setText(ColorSpanHelper.parseSampColors("{7E8B9B}💬 در حال انتظار برای دریافت چت رسمی سرور...\n"), TextView.BufferType.SPANNABLE);
        } else {
            SpannableStringBuilder ssb = new SpannableStringBuilder();
            for (String line : chat.split("\n")) {
                if (line.trim().isEmpty()) continue;
                if (filterSmsAds && BotService.isSmsAd(line)) {
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
        BotService.removeLogCallback(chatLogCallback);
        super.onDestroy();
    }

    private void sendUserCommand(String cmd) {
        if (accountNick != null && !cmd.isEmpty()) {
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
        scrollConsole.post(() -> scrollConsole.fullScroll(ScrollView.FOCUS_DOWN));
    }
}
