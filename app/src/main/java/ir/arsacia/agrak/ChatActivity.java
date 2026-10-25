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
                if (BotService.isStatsDumpLine(line)) {
                    return;
                }
                if (filterSmsAds && BotService.isSmsAd(line)) {
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
            if (accountId != null && (accountId.equals(accId) || (accountNick != null && accountNick.equalsIgnoreCase(accId)))) {
                runOnUiThread(() -> {
                    if (tvChatStatus != null) {
                        String st = "🟢 آنلاین | لول: " + level + " | رسپکت: " + respect;
                        if (!"--".equals(rob) || !"--".equals(escape)) {
                            st += " | راب: " + rob;
                        }
                        tvChatStatus.setText(st);
                    }
                    Toast.makeText(ChatActivity.this, "اطلاعات مشخصات اکانت به‌روزرسانی شد (لول: " + level + "، رسپکت: " + respect + ")", Toast.LENGTH_SHORT).show();
                });
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

        cbFilterSmsAds = findViewById(R.id.cbFilterSmsAds);

        Button btnReconnect = findViewById(R.id.btnQuickReconnect);
        Button btnStats = findViewById(R.id.btnQuickStats);
        Button btnSleep = findViewById(R.id.btnQuickSleep);

        tvName.setText(accountNick != null ? accountNick : "اکانت");
        btnBack.setOnClickListener(v -> finish());

        // Initial sync of cached stats in chat header
        String[] initialStats = BotService.getCachedStats(accountId);
        if (initialStats == null && accountNick != null) {
            initialStats = BotService.getCachedStats(accountNick);
        }
        if (initialStats != null && tvChatStatus != null) {
            String st = "🟢 آنلاین | لول: " + initialStats[0] + " | رسپکت: " + initialStats[1];
            if (!"--".equals(initialStats[2]) || !"--".equals(initialStats[3])) {
                st += " | راب: " + initialStats[2];
            }
            tvChatStatus.setText(st);
        }

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
                if (msg.equalsIgnoreCase("/stats")) {
                    if (accountNick != null) {
                        BotService.sendLocalCommand(accountNick, "/stats", false);
                        Toast.makeText(this, "در حال استعلام و به‌روزرسانی مشخصات اکانت...", Toast.LENGTH_SHORT).show();
                    }
                } else {
                    sendUserCommand(msg);
                }
                etChatMessage.setText("");
            }
        });

        // Quick Command Shortcuts
        btnReconnect.setOnClickListener(v -> sendUserCommand("!reconnect"));
        if (btnStats != null) {
            btnStats.setOnClickListener(v -> {
                if (accountNick != null) {
                    BotService.sendLocalCommand(accountNick, "/stats", false);
                    Toast.makeText(this, "در حال استعلام و به‌روزرسانی مشخصات اکانت...", Toast.LENGTH_SHORT).show();
                }
            });
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
                if (BotService.isStatsDumpLine(line)) {
                    continue;
                }
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
