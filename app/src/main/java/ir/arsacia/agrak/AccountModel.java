package ir.arsacia.agrak;

public class AccountModel {
    public enum Status {
        OFFLINE,
        CONNECTING,
        ONLINE
    }

    private String id;
    private String nickname;
    private String password;
    private boolean autoLogin;
    private boolean antiAfk;

    private Status status = Status.OFFLINE;
    private long connectionStartTime = 0;

    // In-game stats from /stats
    private String level = "--";
    private String respect = "--";
    private String robPoints = "--";
    private String escapePoints = "--";

    public AccountModel(String nickname, String password, boolean autoLogin, boolean antiAfk) {
        this.id = String.valueOf(System.currentTimeMillis()) + "_" + nickname;
        this.nickname = nickname;
        this.password = password;
        this.autoLogin = autoLogin;
        this.antiAfk = antiAfk;
    }

    public String getId() { return id; }
    public String getNickname() { return nickname; }
    public void setNickname(String nickname) { this.nickname = nickname; }

    public String getPassword() { return password; }
    public void setPassword(String password) { this.password = password; }

    public boolean isAutoLogin() { return autoLogin; }
    public void setAutoLogin(boolean autoLogin) { this.autoLogin = autoLogin; }

    public boolean isAntiAfk() { return antiAfk; }
    public void setAntiAfk(boolean antiAfk) { this.antiAfk = antiAfk; }

    public Status getStatus() { return status; }
    public void setStatus(Status status) {
        this.status = status;
        if (status == Status.ONLINE) {
            if (this.connectionStartTime == 0) {
                this.connectionStartTime = System.currentTimeMillis();
            }
        } else {
            this.connectionStartTime = 0;
        }
    }

    public long getConnectionStartTime() { return connectionStartTime; }

    public String getLevel() { return level; }
    public void setLevel(String level) { this.level = level; }

    public String getRespect() { return respect; }
    public void setRespect(String respect) { this.respect = respect; }

    public String getRobPoints() { return robPoints; }
    public void setRobPoints(String robPoints) { this.robPoints = robPoints; }

    public String getEscapePoints() { return escapePoints; }
    public void setEscapePoints(String escapePoints) { this.escapePoints = escapePoints; }
}
