package ir.arsacia.agrak;

import android.content.Context;
import android.graphics.Color;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.animation.AlphaAnimation;
import android.view.animation.Animation;
import android.widget.Button;
import android.widget.ImageButton;
import android.widget.TextView;
import androidx.annotation.NonNull;
import androidx.appcompat.widget.SwitchCompat;
import androidx.recyclerview.widget.RecyclerView;
import java.util.List;
import java.util.Locale;

public class AccountAdapter extends RecyclerView.Adapter<AccountAdapter.ViewHolder> {

    public interface OnAccountActionListener {
        void onToggle(AccountModel account, boolean isChecked);
        void onEnterChat(AccountModel account);
        void onEdit(AccountModel account);
        void onDelete(AccountModel account);
    }

    private final Context context;
    private final List<AccountModel> accounts;
    private final OnAccountActionListener listener;

    public AccountAdapter(Context context, List<AccountModel> accounts, OnAccountActionListener listener) {
        this.context = context;
        this.accounts = accounts;
        this.listener = listener;
    }

    @NonNull
    @Override
    public ViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
        View view = LayoutInflater.from(context).inflate(R.layout.item_account_card, parent, false);
        return new ViewHolder(view);
    }

    @Override
    public void onBindViewHolder(@NonNull ViewHolder holder, int position) {
        AccountModel acc = accounts.get(position);

        holder.tvAccountNick.setText("👤 " + acc.getNickname());

        // Status styling with animated indicator dot
        switch (acc.getStatus()) {
            case ONLINE:
                holder.viewStatusDot.clearAnimation();
                holder.viewStatusDot.setBackgroundResource(R.drawable.bg_dot_online);
                startPulsingAnimation(holder.viewStatusDot, 650);

                holder.tvStatus.setText("🟢 روشن (آنلاین)");
                holder.tvStatus.setTextColor(Color.parseColor("#00E676"));

                holder.switchConnect.setText("🟢 روشن");
                holder.switchConnect.setTextColor(Color.parseColor("#00E676"));
                holder.switchConnect.setChecked(true);
                break;

            case CONNECTING:
                holder.viewStatusDot.clearAnimation();
                holder.viewStatusDot.setBackgroundResource(R.drawable.bg_dot_connecting);
                startPulsingAnimation(holder.viewStatusDot, 350);

                holder.tvStatus.setText("🟡 در حال اتصال...");
                holder.tvStatus.setTextColor(Color.parseColor("#FFD600"));

                holder.switchConnect.setText("🟡 در حال اتصال");
                holder.switchConnect.setTextColor(Color.parseColor("#FFD600"));
                holder.switchConnect.setChecked(true);
                break;

            case OFFLINE:
            default:
                holder.viewStatusDot.clearAnimation();
                holder.viewStatusDot.setBackgroundResource(R.drawable.bg_dot_offline);

                holder.tvStatus.setText("🔴 خاموش (آفلاین)");
                holder.tvStatus.setTextColor(Color.parseColor("#FF5252"));

                holder.switchConnect.setText("🔴 خاموش");
                holder.switchConnect.setTextColor(Color.parseColor("#FF5252"));
                holder.switchConnect.setChecked(false);
                break;
        }

        // Live Uptime
        if (acc.getStatus() == AccountModel.Status.ONLINE && acc.getConnectionStartTime() > 0) {
            long diffSec = (System.currentTimeMillis() - acc.getConnectionStartTime()) / 1000;
            long hours = diffSec / 3600;
            long minutes = (diffSec % 3600) / 60;
            long seconds = diffSec % 60;
            holder.tvUptime.setText(String.format(Locale.getDefault(), "مدت زمان: %02d:%02d:%02d", hours, minutes, seconds));
        } else {
            holder.tvUptime.setText("مدت زمان: 00:00:00");
        }

        // In-game stats with premium gaming emojis
        holder.tvStatLevel.setText("👑 لول: " + acc.getLevel());
        holder.tvStatRespect.setText("💎 رسپکت: " + acc.getRespect());
        holder.tvStatRob.setText("🥷 راب پوینت: " + acc.getRobPoints());
        holder.tvStatEscape.setText("⛓️‍💥 فرار از زندان: " + acc.getEscapePoints());

        // Listeners
        holder.switchConnect.setOnClickListener(v -> {
            if (listener != null) {
                listener.onToggle(acc, holder.switchConnect.isChecked());
            }
        });

        holder.btnEnterChat.setOnClickListener(v -> {
            if (listener != null) listener.onEnterChat(acc);
        });

        holder.btnEditAccount.setOnClickListener(v -> {
            if (listener != null) listener.onEdit(acc);
        });

        holder.btnDeleteAccount.setOnClickListener(v -> {
            if (listener != null) listener.onDelete(acc);
        });
    }

    private void startPulsingAnimation(View view, int durationMs) {
        Animation pulse = new AlphaAnimation(0.25f, 1.0f);
        pulse.setDuration(durationMs);
        pulse.setRepeatMode(Animation.REVERSE);
        pulse.setRepeatCount(Animation.INFINITE);
        view.startAnimation(pulse);
    }

    @Override
    public int getItemCount() {
        return accounts.size();
    }

    public static class ViewHolder extends RecyclerView.ViewHolder {
        View viewStatusDot;
        TextView tvAccountNick, tvStatus, tvUptime;
        TextView tvStatLevel, tvStatRespect, tvStatRob, tvStatEscape;
        SwitchCompat switchConnect;
        Button btnEnterChat;
        ImageButton btnEditAccount, btnDeleteAccount;

        public ViewHolder(@NonNull View itemView) {
            super(itemView);
            viewStatusDot = itemView.findViewById(R.id.viewStatusDot);
            tvAccountNick = itemView.findViewById(R.id.tvAccountNick);
            tvStatus = itemView.findViewById(R.id.tvStatus);
            tvUptime = itemView.findViewById(R.id.tvUptime);
            tvStatLevel = itemView.findViewById(R.id.tvStatLevel);
            tvStatRespect = itemView.findViewById(R.id.tvStatRespect);
            tvStatRob = itemView.findViewById(R.id.tvStatRob);
            tvStatEscape = itemView.findViewById(R.id.tvStatEscape);
            switchConnect = itemView.findViewById(R.id.switchConnect);
            btnEnterChat = itemView.findViewById(R.id.btnEnterChat);
            btnEditAccount = itemView.findViewById(R.id.btnEditAccount);
            btnDeleteAccount = itemView.findViewById(R.id.btnDeleteAccount);
        }
    }
}
