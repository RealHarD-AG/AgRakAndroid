package ir.arsacia.agrak;

import android.graphics.Color;
import android.text.Spannable;
import android.text.SpannableString;
import android.text.SpannableStringBuilder;
import android.text.style.ForegroundColorSpan;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class ColorSpanHelper {
    private static final Pattern COLOR_TAG_PATTERN = Pattern.compile("\\{([0-9a-fA-F]{6})\\}");
    private static final int DEFAULT_COLOR = Color.parseColor("#E0E0E0");
    private static final int TIMESTAMP_COLOR = Color.parseColor("#7E8B9B");

    /**
     * Parses SA:MP embedded color tags like {RRGGBB} into styled Android Spannable.
     * Extracts and styles timestamp [HH:mm:ss] in subtle gray, and preserves colors.
     */
    public static SpannableStringBuilder parseSampColors(String rawLine) {
        if (rawLine == null) {
            return new SpannableStringBuilder("");
        }

        SpannableStringBuilder ssb = new SpannableStringBuilder();
        String text = rawLine;

        // Extract timestamp [HH:mm:ss] or [HH:mm:ss.mmm] if present at start
        int timeEnd = text.indexOf("] ");
        if (text.startsWith("[") && timeEnd > 0 && timeEnd <= 15) {
            String timeStr = text.substring(0, timeEnd + 2);
            SpannableString timeSpannable = new SpannableString(timeStr);
            timeSpannable.setSpan(new ForegroundColorSpan(TIMESTAMP_COLOR), 0, timeStr.length(), Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
            ssb.append(timeSpannable);
            text = text.substring(timeEnd + 2);
        }

        Matcher matcher = COLOR_TAG_PATTERN.matcher(text);
        int lastEnd = 0;
        int currentColor = DEFAULT_COLOR;

        while (matcher.find()) {
            int matchStart = matcher.start();
            int matchEnd = matcher.end();

            // Append segment before color tag with previous color
            if (matchStart > lastEnd) {
                String segment = text.substring(lastEnd, matchStart);
                SpannableString span = new SpannableString(segment);
                span.setSpan(new ForegroundColorSpan(currentColor), 0, segment.length(), Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
                ssb.append(span);
            }

            // Extract and parse hex color
            String hex = matcher.group(1);
            try {
                int parsed = Color.parseColor("#" + hex);
                // Prevent unreadable pitch-black text on dark console background
                int r = Color.red(parsed);
                int g = Color.green(parsed);
                int b = Color.blue(parsed);
                if (r < 35 && g < 35 && b < 35) {
                    parsed = Color.rgb(Math.max(r, 65), Math.max(g, 65), Math.max(b, 65));
                }
                currentColor = parsed;
            } catch (Exception ignored) {
                currentColor = DEFAULT_COLOR;
            }

            lastEnd = matchEnd;
        }

        // Append remaining text after last tag
        if (lastEnd < text.length()) {
            String remaining = text.substring(lastEnd);
            SpannableString span = new SpannableString(remaining);
            span.setSpan(new ForegroundColorSpan(currentColor), 0, remaining.length(), Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
            ssb.append(span);
        }

        return ssb;
    }
}
