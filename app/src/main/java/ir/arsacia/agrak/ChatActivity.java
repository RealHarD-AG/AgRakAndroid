package ir.arsacia.agrak;

import android.os.Bundle;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ImageButton;
import android.widget.ScrollView;
import android.widget.TextView;
import androidx.appcompat.app.AppCompatActivity;

public class ChatActivity extends AppCompatActivity {

    private String accountId;
    private String accountNick;
    private TextView tvConsoleLogs;
    private ScrollView scrollConsole;
    private EditText etChatMessage;

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

        Button btnReconnect = findViewById(R.id.btnQuickReconnect);
        Button btnSleep = findViewById(R.id.btnQuickSleep);

        tvName.setText("👤 " + (accountNick != null ? accountNick : "اکانت"));
        btnBack.setOnClickListener(v -> finish());

        // Load existing logs
        if (accountId != null) {
            String logs = BotService.getLogsForAccount(accountId);
            if (!logs.isEmpty()) {
                tvConsoleLogs.setText(logs);
                scrollToBottom();
            }
        }

        // Send chat message
        btnSend.setOnClickListener(v -> {
            String msg = etChatMessage.getText().toString().trim();
            if (!msg.isEmpty()) {
                sendCommand(msg);
                etChatMessage.setText("");
            }
        });

        // Quick Command Shortcuts
        btnReconnect.setOnClickListener(v -> sendCommand("!reconnect"));
        btnSleep.setOnClickListener(v -> sendCommand("/sleep"));

        // Listen for new logs in real-time
        BotService.setLogCallback(new BotService.LogCallback() {
            @Override
            public void onNewLog(String accId, String line) {
                if (accountId != null && accountId.equals(accId)) {
                    runOnUiThread(() -> {
                        tvConsoleLogs.append(line + "\n");
                        scrollToBottom();
                    });
                }
            }

            @Override
            public void onStatsUpdated(String accId, String level, String respect, String rob, String escape) {}
        });
    }

    private void sendCommand(String cmd) {
        if (accountId != null) {
            BotService.appendLog(accountId, "> " + cmd);
            BotService.sendRemoteCommand(accountNick, cmd);
            scrollToBottom();
        }
    }

    private void scrollToBottom() {
        scrollConsole.post(() -> scrollConsole.fullScroll(ScrollView.FOCUS_DOWN));
    }
}
