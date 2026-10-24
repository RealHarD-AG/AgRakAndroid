package ir.arsacia.agrak;

import android.content.Context;
import android.graphics.Color;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.animation.AlphaAnimation;
import android.view.animation.Animation;
import android.view.animation.AnimationSet;
import android.view.animation.LinearInterpolator;
import android.view.animation.RotateAnimation;
import android.view.animation.ScaleAnimation;
import android.widget.Button;
import android.widget.ImageButton;
import android.widget.ImageView;
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

        // Status styling with animated 3D Telegram indicator
        switch (acc.getStatus()) {
            case ONLINE:
                holder.imgStatusIcon.clearAnimation();
                holder.imgStatusIcon.setImageResource(R.drawable.tg_online);
                startBreathingPulse(holder.imgStatusIcon);

                holder.tvStatus.setText("روشن (آنلاین)");
                holder.tvStatus.setTextColor(Color.parseColor("#00E676"));

                holder.imgSwitchIcon.setImageResource(R.drawable.tg_switch_on);
                holder.switchConnect.setText("روشن (فعال)");
                holder.switchConnect.setTextColor(Color.parseColor("#00E676"));
                holder.switchConnect.setChecked(true);
                break;

            case CONNECTING:
                holder.imgStatusIcon.clearAnimation();
                holder.imgStatusIcon.setImageResource(R.drawable.tg_online);
                startSpinAnimation(holder.imgStatusIcon);

                holder.tvStatus.setText("در حال اتصال...");
                holder.tvStatus.setTextColor(Color.parseColor("#FFD600"));

                holder.imgSwitchIcon.setImageResource(R.drawable.tg_switch_on);
                holder.switchConnect.setText("در حال اتصال");
                holder.switchConnect.setTextColor(Color.parseColor("#FFD600"));
                holder.switchConnect.setChecked(true);
                break;

            case OFFLINE:
            default:
                holder.imgStatusIcon.clearAnimation();
                holder.imgStatusIcon.setImageResource(R.drawable.tg_offline);

                holder.tvStatus.setText("خاموش (آفلاین)");
                holder.tvStatus.setTextColor(Color.parseColor("#FF5252"));

                holder.imgSwitchIcon.setImageResource(R.drawable.tg_switch_off);
                holder.switchConnect.setText("خاموش (غیرفعال)");
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

        // In-game stats with Telegram Premium 3D custom badges
        holder.tvStatLevel.setText("لول: " + acc.getLevel());
        holder.tvStatRespect.setText("رسپکت: " + acc.getRespect());
        holder.tvStatRob.setText("سرقت: " + acc.getRobPoints());
        holder.tvStatEscape.setText("فرار: " + acc.getEscapePoints());

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

    private void startBreathingPulse(View view) {
        AnimationSet set = new AnimationSet(true);
        ScaleAnimation scale = new ScaleAnimation(0.85f, 1.15f, 0.85f, 1.15f,
                Animation.RELATIVE_TO_SELF, 0.5f, Animation.RELATIVE_TO_SELF, 0.5f);
        scale.setDuration(700);
        scale.setRepeatMode(Animation.REVERSE);
        scale.setRepeatCount(Animation.INFINITE);

        AlphaAnimation alpha = new AlphaAnimation(0.6f, 1.0f);
        alpha.setDuration(700);
        alpha.setRepeatMode(Animation.REVERSE);
        alpha.setRepeatCount(Animation.INFINITE);

        set.addAnimation(scale);
        set.addAnimation(alpha);
        view.startAnimation(set);
    }

    private void startSpinAnimation(View view) {
        RotateAnimation rotate = new RotateAnimation(0, 360,
                Animation.RELATIVE_TO_SELF, 0.5f, Animation.RELATIVE_TO_SELF, 0.5f);
        rotate.setDuration(900);
        rotate.setRepeatCount(Animation.INFINITE);
        rotate.setInterpolator(new LinearInterpolator());
        view.startAnimation(rotate);
    }

    @Override
    public int getItemCount() {
        return accounts.size();
    }

    public static class ViewHolder extends RecyclerView.ViewHolder {
        ImageView imgStatusIcon, imgSwitchIcon;
        TextView tvAccountNick, tvStatus, tvUptime;
        TextView tvStatLevel, tvStatRespect, tvStatRob, tvStatEscape;
        SwitchCompat switchConnect;
        Button btnEnterChat;
        ImageButton btnEditAccount, btnDeleteAccount;

        public ViewHolder(@NonNull View itemView) {
            super(itemView);
            imgStatusIcon = itemView.findViewById(R.id.imgStatusIcon);
            imgSwitchIcon = itemView.findViewById(R.id.imgSwitchIcon);
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
