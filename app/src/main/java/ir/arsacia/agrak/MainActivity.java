package ir.arsacia.agrak;

import android.app.AlertDialog;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.Toast;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.SwitchCompat;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import org.json.JSONArray;
import org.json.JSONObject;
import java.util.ArrayList;
import java.util.List;

public class MainActivity extends AppCompatActivity implements AccountAdapter.OnAccountActionListener {

    private final List<AccountModel> accounts = new ArrayList<>();
    private AccountAdapter adapter;
    private TextView tvEmptyAccounts;
    private final Handler timerHandler = new Handler(Looper.getMainLooper());
    private SharedPreferences prefs;

    private final Runnable timerRunnable = new Runnable() {
        @Override
        public void run() {
            if (adapter != null) {
                adapter.notifyDataSetChanged();
            }
            timerHandler.postDelayed(this, 1000);
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        prefs = getSharedPreferences("AGRak_Prefs", MODE_PRIVATE);

        RecyclerView rvAccounts = findViewById(R.id.rvAccounts);
        tvEmptyAccounts = findViewById(R.id.tvEmptyAccounts);
        Button btnAddAccount = findViewById(R.id.btnAddAccount);
        Button btnStartAll = findViewById(R.id.btnStartAll);
        Button btnStopAll = findViewById(R.id.btnStopAll);

        adapter = new AccountAdapter(this, accounts, this);
        rvAccounts.setLayoutManager(new LinearLayoutManager(this));
        rvAccounts.setAdapter(adapter);

        loadAccounts();
        updateEmptyView();

        btnAddAccount.setOnClickListener(v -> showAccountEditorDialog(null));

        btnStartAll.setOnClickListener(v -> {
            for (AccountModel acc : accounts) {
                startBot(acc);
            }
            adapter.notifyDataSetChanged();
            Toast.makeText(this, "دستور اتصال تمام اکانت‌ها ارسال شد", Toast.LENGTH_SHORT).show();
        });

        btnStopAll.setOnClickListener(v -> {
            for (AccountModel acc : accounts) {
                stopBot(acc);
            }
            adapter.notifyDataSetChanged();
            Toast.makeText(this, "تمام اکانت‌ها قطع شدند", Toast.LENGTH_SHORT).show();
        });

        // Register Log/Stats listener from BotService
        BotService.setLogCallback(new BotService.LogCallback() {
            @Override
            public void onNewLog(String accountId, String line) {}

            @Override
            public void onStatsUpdated(String accountId, String level, String respect, String rob, String escape) {
                runOnUiThread(() -> {
                    for (AccountModel acc : accounts) {
                        if (acc.getId().equals(accountId)) {
                            acc.setLevel(level);
                            acc.setRespect(respect);
                            acc.setRobPoints(rob);
                            acc.setEscapePoints(escape);
                            adapter.notifyDataSetChanged();
                            break;
                        }
                    }
                });
            }
        });

        startBotService();
    }

    private void startBotService() {
        Intent serviceIntent = new Intent(this, BotService.class);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            startForegroundService(serviceIntent);
        } else {
            startService(serviceIntent);
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        timerHandler.post(timerRunnable);
    }

    @Override
    protected void onPause() {
        super.onPause();
        timerHandler.removeCallbacks(timerRunnable);
    }

    private void updateEmptyView() {
        tvEmptyAccounts.setVisibility(accounts.isEmpty() ? View.VISIBLE : View.GONE);
    }

    private void showAccountEditorDialog(AccountModel accountToEdit) {
        View dialogView = LayoutInflater.from(this).inflate(R.layout.dialog_account_editor, null);
        AlertDialog dialog = new AlertDialog.Builder(this)
                .setView(dialogView)
                .setCancelable(false)
                .create();

        TextView tvTitle = dialogView.findViewById(R.id.tvDialogTitle);
        EditText etNick = dialogView.findViewById(R.id.etNickname);
        EditText etPass = dialogView.findViewById(R.id.etPassword);
        SwitchCompat swAutoLogin = dialogView.findViewById(R.id.switchAutoLogin);
        SwitchCompat swAntiAfk = dialogView.findViewById(R.id.switchAntiAfk);
        Button btnSave = dialogView.findViewById(R.id.btnSaveAccount);
        Button btnCancel = dialogView.findViewById(R.id.btnCancelDialog);

        if (accountToEdit != null) {
            tvTitle.setText(R.string.dialog_edit_title);
            etNick.setText(accountToEdit.getNickname());
            etPass.setText(accountToEdit.getPassword());
            swAutoLogin.setChecked(accountToEdit.isAutoLogin());
            swAntiAfk.setChecked(accountToEdit.isAntiAfk());
        } else {
            tvTitle.setText(R.string.dialog_add_title);
        }

        btnSave.setOnClickListener(v -> {
            String nick = etNick.getText().toString().trim();
            String pass = etPass.getText().toString().trim();

            if (nick.isEmpty()) {
                etNick.setError("لطفاً نام اکانت را وارد کنید");
                return;
            }

            if (accountToEdit != null) {
                accountToEdit.setNickname(nick);
                accountToEdit.setPassword(pass);
                accountToEdit.setAutoLogin(swAutoLogin.isChecked());
                accountToEdit.setAntiAfk(swAntiAfk.isChecked());
            } else {
                AccountModel newAcc = new AccountModel(nick, pass, swAutoLogin.isChecked(), swAntiAfk.isChecked());
                accounts.add(newAcc);
            }

            saveAccounts();
            adapter.notifyDataSetChanged();
            updateEmptyView();
            dialog.dismiss();
            Toast.makeText(this, "اکانت با موفقیت ثبت شد", Toast.LENGTH_SHORT).show();
        });

        btnCancel.setOnClickListener(v -> dialog.dismiss());
        dialog.show();
    }

    @Override
    public void onToggle(AccountModel account, boolean isChecked) {
        if (isChecked) {
            startBot(account);
        } else {
            stopBot(account);
        }
        adapter.notifyDataSetChanged();
    }

    private void startBot(AccountModel acc) {
        acc.setStatus(AccountModel.Status.CONNECTING);
        BotService.appendLog(acc.getId(), "[AGRak] در حال برقراری ارتباط با 185.112.33.99:7777...");
        BotService.appendLog(acc.getId(), "[AGRak] متصل شد. نام اکانت: " + acc.getNickname());

        // Simulation / native hook trigger
        new Handler().postDelayed(() -> {
            acc.setStatus(AccountModel.Status.ONLINE);
            BotService.appendLog(acc.getId(), "[ArSaCiA] ورود با موفقیت انجام شد و اسپاون صورت گرفت.");
            BotService.appendLog(acc.getId(), "[AutoStats] استعلام آمار اکانت انجام شد.");
            
            // Initial mock / trigger stats
            if (acc.getLevel().equals("--")) {
                BotService.updateStats(acc.getId(), "18", "24/36", "3", "5");
            }
            adapter.notifyDataSetChanged();
        }, 2000);
    }

    private void stopBot(AccountModel acc) {
        acc.setStatus(AccountModel.Status.OFFLINE);
        BotService.appendLog(acc.getId(), "[AGRak] اتصال از سرور قطع شد.");
    }

    @Override
    public void onEnterChat(AccountModel account) {
        Intent intent = new Intent(this, ChatActivity.class);
        intent.putExtra("account_id", account.getId());
        intent.putExtra("account_nick", account.getNickname());
        startActivity(intent);
    }

    @Override
    public void onEdit(AccountModel account) {
        showAccountEditorDialog(account);
    }

    @Override
    public void onDelete(AccountModel account) {
        new AlertDialog.Builder(this)
                .setTitle(R.string.delete_confirm_title)
                .setMessage(getString(R.string.delete_confirm_message, account.getNickname()))
                .setPositiveButton("بله، حذف کن", (dialog, which) -> {
                    stopBot(account);
                    accounts.remove(account);
                    saveAccounts();
                    adapter.notifyDataSetChanged();
                    updateEmptyView();
                    Toast.makeText(this, "اکانت حذف شد", Toast.LENGTH_SHORT).show();
                })
                .setNegativeButton("انصراف", null)
                .show();
    }

    private void saveAccounts() {
        try {
            JSONArray arr = new JSONArray();
            for (AccountModel acc : accounts) {
                JSONObject obj = new JSONObject();
                obj.put("nick", acc.getNickname());
                obj.put("pass", acc.getPassword());
                obj.put("autoLogin", acc.isAutoLogin());
                obj.put("antiAfk", acc.isAntiAfk());
                arr.put(obj);
            }
            prefs.edit().putString("saved_accounts", arr.toString()).apply();
        } catch (Exception ignored) {}
    }

    private void loadAccounts() {
        accounts.clear();
        String json = prefs.getString("saved_accounts", null);
        if (json != null) {
            try {
                JSONArray arr = new JSONArray(json);
                for (int i = 0; i < arr.length(); i++) {
                    JSONObject obj = arr.getJSONObject(i);
                    AccountModel acc = new AccountModel(
                            obj.getString("nick"),
                            obj.getString("pass"),
                            obj.optBoolean("autoLogin", true),
                            obj.optBoolean("antiAfk", true)
                    );
                    accounts.add(acc);
                }
            } catch (Exception ignored) {}
        } else {
            // Default sample accounts
            accounts.add(new AccountModel("HarD", "Sinaguard9090r4", true, true));
            accounts.add(new AccountModel("SynA", "", true, true));
        }
    }
}
