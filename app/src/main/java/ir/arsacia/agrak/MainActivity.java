package ir.arsacia.agrak;

import android.Manifest;
import android.app.AlertDialog;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ImageView;
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
    private final java.util.Map<String, Long> startingBots = new java.util.concurrent.ConcurrentHashMap<>();

    private final Runnable timerRunnable = new Runnable() {
        @Override
        public void run() {
            boolean changed = false;
            long now = System.currentTimeMillis();

            for (AccountModel acc : accounts) {
                String key = acc.getNickname().toLowerCase();
                boolean running = BotService.isBotRunning(acc.getNickname());

                if (running) {
                    startingBots.remove(key);
                    if (acc.getStatus() != AccountModel.Status.ONLINE) {
                        acc.setStatus(AccountModel.Status.ONLINE);
                        changed = true;
                    }
                } else {
                    Long startAttempt = startingBots.get(key);
                    if (startAttempt != null && (now - startAttempt) < 7000) {
                        // Still in grace period for process to spawn
                        if (acc.getStatus() != AccountModel.Status.CONNECTING) {
                            acc.setStatus(AccountModel.Status.CONNECTING);
                            changed = true;
                        }
                    } else {
                        startingBots.remove(key);
                        if (acc.getStatus() != AccountModel.Status.OFFLINE) {
                            acc.setStatus(AccountModel.Status.OFFLINE);
                            changed = true;
                        }
                    }
                }

                // Sync stats from BotService cache
                String[] stats = BotService.getCachedStats(acc.getId());
                if (stats == null) {
                    stats = BotService.getCachedStats(acc.getNickname());
                }
                if (stats != null) {
                    if (!"--".equals(stats[0]) && !stats[0].equals(acc.getLevel())) {
                        acc.setLevel(stats[0]);
                        changed = true;
                    }
                    if (!"--".equals(stats[1]) && !stats[1].equals(acc.getRespect())) {
                        acc.setRespect(stats[1]);
                        changed = true;
                    }
                    if (!"--".equals(stats[2]) && !stats[2].equals(acc.getRobPoints())) {
                        acc.setRobPoints(stats[2]);
                        changed = true;
                    }
                    if (!"--".equals(stats[3]) && !stats[3].equals(acc.getEscapePoints())) {
                        acc.setEscapePoints(stats[3]);
                        changed = true;
                    }
                }
            }
            if (adapter != null) {
                adapter.notifyDataSetChanged();
            }
            if (changed) {
                saveAccounts();
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
                    if (acc.getId().equals(accountId) || (acc.getNickname() != null && acc.getNickname().equalsIgnoreCase(accountId))) {
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
                    if (adapter != null) {
                        adapter.notifyDataSetChanged();
                    }
                }
            });
        }

        @Override
        public void onConnected(String accountId) {
            runOnUiThread(() -> {
                boolean changed = false;
                for (AccountModel acc : accounts) {
                    if (acc.getId().equals(accountId) || (acc.getNickname() != null && acc.getNickname().equalsIgnoreCase(accountId))) {
                        if (acc.getStatus() != AccountModel.Status.ONLINE) {
                            acc.setStatus(AccountModel.Status.ONLINE);
                            changed = true;
                        }
                        break;
                    }
                }
                if (changed && adapter != null) {
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

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            if (checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED) {
                requestPermissions(new String[]{Manifest.permission.POST_NOTIFICATIONS}, 101);
            }
        }

        btnAddAccount.setOnClickListener(v -> showAccountEditorDialog(null));

        btnStartAll.setOnClickListener(v -> {
            for (AccountModel acc : accounts) {
                startBot(acc);
            }
            adapter.notifyDataSetChanged();
            Toast.makeText(this, "اتصال تمام اکانت‌ها با اینترنت گوشی آغاز شد", Toast.LENGTH_SHORT).show();
        });

        btnStopAll.setOnClickListener(v -> {
            for (AccountModel acc : accounts) {
                stopBot(acc);
            }
            adapter.notifyDataSetChanged();
            Toast.makeText(this, "تمام اکانت‌ها قطع شدند", Toast.LENGTH_SHORT).show();
        });

        // Animate footer heartbeat
        ImageView imgFooterHeart = findViewById(R.id.imgFooterHeart);
        if (imgFooterHeart != null) {
            android.view.animation.ScaleAnimation heartbeat = new android.view.animation.ScaleAnimation(
                    0.80f, 1.22f,
                    0.80f, 1.22f,
                    android.view.animation.Animation.RELATIVE_TO_SELF, 0.5f,
                    android.view.animation.Animation.RELATIVE_TO_SELF, 0.5f
            );
            heartbeat.setDuration(600);
            heartbeat.setRepeatMode(android.view.animation.Animation.REVERSE);
            heartbeat.setRepeatCount(android.view.animation.Animation.INFINITE);
            heartbeat.setInterpolator(new android.view.animation.AccelerateDecelerateInterpolator());
            imgFooterHeart.startAnimation(heartbeat);
        }

        BotService.addLogCallback(mainLogCallback);
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
        // Check running status & sync cached stats
        long now = System.currentTimeMillis();
        for (AccountModel acc : accounts) {
            String key = acc.getNickname().toLowerCase();
            boolean running = BotService.isBotRunning(acc.getNickname());
            if (running) {
                startingBots.remove(key);
                acc.setStatus(AccountModel.Status.ONLINE);
            } else {
                Long startAttempt = startingBots.get(key);
                if (startAttempt == null || (now - startAttempt) >= 7000) {
                    startingBots.remove(key);
                    acc.setStatus(AccountModel.Status.OFFLINE);
                }
            }
            String[] stats = BotService.getCachedStats(acc.getId());
            if (stats == null) {
                stats = BotService.getCachedStats(acc.getNickname());
            }
            if (stats != null) {
                if (!"--".equals(stats[0])) acc.setLevel(stats[0]);
                if (!"--".equals(stats[1])) acc.setRespect(stats[1]);
                if (!"--".equals(stats[2])) acc.setRobPoints(stats[2]);
                if (!"--".equals(stats[3])) acc.setEscapePoints(stats[3]);
            }
        }
        if (adapter != null) {
            adapter.notifyDataSetChanged();
        }
        timerHandler.post(timerRunnable);
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
        ImageView btnTogglePass = dialogView.findViewById(R.id.btnTogglePassword);
        SwitchCompat swAutoLogin = dialogView.findViewById(R.id.switchAutoLogin);
        Button btnSave = dialogView.findViewById(R.id.btnSaveAccount);
        Button btnCancel = dialogView.findViewById(R.id.btnCancelDialog);

        final boolean[] isPassVisible = {false};
        if (btnTogglePass != null) {
            btnTogglePass.setOnClickListener(v -> {
                isPassVisible[0] = !isPassVisible[0];
                if (isPassVisible[0]) {
                    etPass.setTransformationMethod(android.text.method.HideReturnsTransformationMethod.getInstance());
                    btnTogglePass.setImageResource(R.drawable.ic_visibility);
                } else {
                    etPass.setTransformationMethod(android.text.method.PasswordTransformationMethod.getInstance());
                    btnTogglePass.setImageResource(R.drawable.ic_visibility_off);
                }
                etPass.setSelection(etPass.getText().length());
            });
        }

        if (accountToEdit != null) {
            tvTitle.setText(R.string.dialog_edit_title);
            etNick.setText(accountToEdit.getNickname());
            etPass.setText(accountToEdit.getPassword());
            swAutoLogin.setChecked(accountToEdit.isAutoLogin());
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

            if (pass.isEmpty() && swAutoLogin.isChecked()) {
                etPass.setError("لطفاً رمز عبور اکانت را وارد کنید");
                return;
            }

            if (accountToEdit != null) {
                accountToEdit.setNickname(nick);
                accountToEdit.setPassword(pass);
                accountToEdit.setAutoLogin(swAutoLogin.isChecked());
                accountToEdit.setAntiAfk(true);
            } else {
                AccountModel newAcc = new AccountModel(nick, pass, swAutoLogin.isChecked(), true);
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
        startingBots.put(acc.getNickname().toLowerCase(), System.currentTimeMillis());
        acc.setStatus(AccountModel.Status.CONNECTING);
        saveAccounts();
        adapter.notifyDataSetChanged();

        // Start local native RakSAMP on phone using phone network
        BotService.startLocalBot(this, acc);
    }

    private void stopBot(AccountModel acc) {
        startingBots.remove(acc.getNickname().toLowerCase());
        acc.setStatus(AccountModel.Status.OFFLINE);
        saveAccounts();
        adapter.notifyDataSetChanged();

        // Stop local native process
        BotService.stopLocalBot(acc.getNickname());
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
                            true
                    );
                    acc.setAntiAfk(true);
                    if (obj.has("id")) {
                        acc.setId(obj.getString("id"));
                    }
                    // Initial status is always OFFLINE unless process is currently running locally
                    boolean isRunning = BotService.isBotRunning(acc.getNickname());
                    acc.setStatus(isRunning ? AccountModel.Status.ONLINE : AccountModel.Status.OFFLINE);

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