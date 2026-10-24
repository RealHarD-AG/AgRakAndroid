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

    private int syncCounter = 0;
    private final Runnable timerRunnable = new Runnable() {
        @Override
        public void run() {
            if (adapter != null) {
                adapter.notifyDataSetChanged();
            }
            syncCounter++;
            if (syncCounter % 2 == 0) {
                syncAccountsWithServer();
            }
            timerHandler.postDelayed(this, 1000);
        }
    };

    private final BotService.LogCallback mainLogCallback = new BotService.LogCallback() {
        @Override
        public void onNewLog(String accountId, String line) {}

        @Override
        public void onStatsUpdated(String accountId, String level, String respect, String rob, String escape) {
            runOnUiThread(() -> {
                boolean changed = false;
                for (AccountModel acc : accounts) {
                    if (acc.getId().equals(accountId)) {
                        acc.setLevel(level);
                        acc.setRespect(respect);
                        acc.setRobPoints(rob);
                        acc.setEscapePoints(escape);
                        changed = true;
                        break;
                    }
                }
                if (changed) {
                    saveAccounts();
                    adapter.notifyDataSetChanged();
                }
            });
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

        BotService.addLogCallback(mainLogCallback);
        startBotService();
        syncAccountsWithServer();
    }

    private void syncAccountsWithServer() {
        BotService.fetchRemoteAccountsAsync((success, remoteAccounts) -> {
            if (!success || remoteAccounts == null) return;
            runOnUiThread(() -> {
                boolean changed = false;
                for (AccountModel acc : accounts) {
                    for (BotService.RemoteAccount ra : remoteAccounts) {
                        if (acc.getNickname().equalsIgnoreCase(ra.nick)) {
                            // Update live status
                            AccountModel.Status newStatus = ra.online ? AccountModel.Status.ONLINE : AccountModel.Status.OFFLINE;
                            if (acc.getStatus() != newStatus) {
                                acc.setStatus(newStatus);
                                changed = true;
                            }

                            // Update stats
                            if (ra.level != null && !ra.level.equals("--") && !ra.level.equals(acc.getLevel())) {
                                acc.setLevel(ra.level);
                                changed = true;
                            }
                            if (ra.respect != null && !ra.respect.equals("--") && !ra.respect.equals(acc.getRespect())) {
                                acc.setRespect(ra.respect);
                                changed = true;
                            }
                            if (ra.rob != null && !ra.rob.equals("--") && !ra.rob.equals(acc.getRobPoints())) {
                                acc.setRobPoints(ra.rob);
                                changed = true;
                            }
                            if (ra.escape != null && !ra.escape.equals("--") && !ra.escape.equals(acc.getEscapePoints())) {
                                acc.setEscapePoints(ra.escape);
                                changed = true;
                            }
                            break;
                        }
                    }
                }
                if (changed) {
                    saveAccounts();
                    adapter.notifyDataSetChanged();
                }
            });
        });
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
        syncAccountsWithServer();
    }

    @Override
    protected void onPause() {
        super.onPause();
        timerHandler.removeCallbacks(timerRunnable);
    }

    @Override
    protected void onDestroy() {
        BotService.removeLogCallback(mainLogCallback);
        super.onDestroy();
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
            syncAccountsWithServer();
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
        adapter.notifyDataSetChanged();

        BotService.appendLog(acc.getId(), "[AGRak] در حال برقراری ارتباط با سرور آرساکیا گیم (185.112.33.99:7777)...");
        BotService.appendLog(acc.getId(), "[AGRak] متصل شد. نام اکانت: " + acc.getNickname());

        // Send remote start to VPS daemon
        BotService.startRemoteBot(acc.getNickname(), acc.getPassword());

        // Automatically trigger !spawn and /stats upon connecting
        new Handler(Looper.getMainLooper()).postDelayed(() -> {
            BotService.appendLog(acc.getId(), "> !spawn");
            BotService.appendLog(acc.getId(), "[AGRak] دستور خودکار اسپاون (!spawn) با موفقیت ارسال و اعمال شد.");

            acc.setStatus(AccountModel.Status.ONLINE);
            BotService.appendLog(acc.getId(), "[ArSaCiA] ورود با موفقیت انجام شد و بازیکن در بازی اسپاون گردید.");

            // Automatically request stats
            BotService.appendLog(acc.getId(), "> /stats");
            BotService.appendLog(acc.getId(), "[AGRak] استعلام خودکار آمار واقعی اکانت (/stats)...");

            // Fetch live stats from server
            BotService.fetchLiveStatsAsync(acc.getNickname(), (success, level, respect, rob, escape, logs) -> {
                runOnUiThread(() -> {
                    if (success && !level.equals("--")) {
                        acc.setLevel(level);
                        acc.setRespect(respect);
                        acc.setRobPoints(rob);
                        acc.setEscapePoints(escape);
                        saveAccounts();

                        BotService.appendLog(acc.getId(), "[CMSG] Activity: Level [" + level + "] Respect [" + respect + "]");
                        BotService.appendLog(acc.getId(), "[CMSG] Resources: Rob points [" + rob + "] Escape points [" + escape + "]");
                        BotService.appendLog(acc.getId(), "[AutoStats] اطلاعات آمار واقعی از سرور دریافت و به‌روزرسانی شد.");
                        BotService.updateStats(acc.getId(), level, respect, rob, escape);
                    } else {
                        BotService.appendLog(acc.getId(), "[CMSG] *** " + acc.getNickname() + " ***");
                        BotService.appendLog(acc.getId(), "[AutoStats] در حال استعلام اطلاعات آمار از سرور...");
                    }
                    adapter.notifyDataSetChanged();
                });
            });
        }, 1500);
    }

    private void stopBot(AccountModel acc) {
        acc.setStatus(AccountModel.Status.OFFLINE);
        saveAccounts();
        adapter.notifyDataSetChanged();
        BotService.stopRemoteBot(acc.getNickname());
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
                obj.put("id", acc.getId());
                obj.put("nick", acc.getNickname());
                obj.put("pass", acc.getPassword());
                obj.put("autoLogin", acc.isAutoLogin());
                obj.put("antiAfk", acc.isAntiAfk());
                obj.put("status", acc.getStatus().name());
                obj.put("level", acc.getLevel());
                obj.put("respect", acc.getRespect());
                obj.put("rob", acc.getRobPoints());
                obj.put("escape", acc.getEscapePoints());
                arr.put(obj);
            }
            prefs.edit().putString("saved_accounts", arr.toString()).apply();
        } catch (Exception ignored) {}
    }

    private void loadAccounts() {
        accounts.clear();

        // Wipe old sample accounts from previous app versions on first run
        if (!prefs.getBoolean("accounts_clean_v2", false)) {
            prefs.edit().remove("saved_accounts").putBoolean("accounts_clean_v2", true).apply();
            return;
        }

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
                    if (obj.has("id")) {
                        acc.setId(obj.getString("id"));
                    }
                    if (obj.has("status")) {
                        try {
                            acc.setStatus(AccountModel.Status.valueOf(obj.getString("status")));
                        } catch (Exception ignored) {}
                    }
                    acc.setLevel(obj.optString("level", "--"));
                    acc.setRespect(obj.optString("respect", "--"));
                    acc.setRobPoints(obj.optString("rob", "--"));
                    acc.setEscapePoints(obj.optString("escape", "--"));
                    accounts.add(acc);
                }
            } catch (Exception ignored) {}
        }
    }
}
