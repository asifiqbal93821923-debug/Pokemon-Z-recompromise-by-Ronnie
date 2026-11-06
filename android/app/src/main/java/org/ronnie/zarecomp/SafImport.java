package org.ronnie.zarecomp;

import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.provider.OpenableColumns;
import android.os.StatFs;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.security.MessageDigest;
import java.util.Locale;
import java.util.UUID;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;
import java.nio.file.AtomicMoveNotSupportedException;

final class SafImport {
    private SafImport() {}

    static String copyIntoPrivateStorage(Context context, Uri uri) {
        File importDir = new File(context.getFilesDir(), "za-import");
        File temp = null;
        File destination = null;
        try {
            if (!importDir.exists() && !importDir.mkdirs())
                return "SAF_IMPORT result=FAIL stage=create_private_dir";
            if (!importDir.isDirectory() || Files.isSymbolicLink(importDir.toPath()))
                return "SAF_IMPORT result=FAIL stage=private_dir_safety_check";

            String displayName = "document.bin";
            long reportedSize = -1L;
            try (Cursor cursor = context.getContentResolver().query(
                    uri, new String[]{OpenableColumns.DISPLAY_NAME, OpenableColumns.SIZE},
                    null, null, null)) {
                if (cursor != null && cursor.moveToFirst()) {
                    int nameCol = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME);
                    int sizeCol = cursor.getColumnIndex(OpenableColumns.SIZE);
                    if (nameCol >= 0 && !cursor.isNull(nameCol)) displayName = cursor.getString(nameCol);
                    if (sizeCol >= 0 && !cursor.isNull(sizeCol)) reportedSize = cursor.getLong(sizeCol);
                }
            }
            String safeName = safeFileName(displayName);
            StatFs stat = new StatFs(importDir.getAbsolutePath());
            long available = stat.getAvailableBytes();
            final long reserveBytes = 16L * 1024L * 1024L;
            if (reportedSize >= 0 && reportedSize > Math.max(0L, available - reserveBytes))
                return "SAF_IMPORT result=FAIL stage=free_space reported_bytes=" + reportedSize
                        + " available_bytes=" + available;

            String unique = UUID.randomUUID().toString();
            temp = new File(importDir, "." + unique + ".part");
            destination = new File(importDir, unique + "_" + safeName);
            MessageDigest digest = MessageDigest.getInstance("SHA-256");
            long total = 0L;
            byte[] buffer = new byte[1024 * 1024];
            try (InputStream input = context.getContentResolver().openInputStream(uri)) {
                if (input == null) return "SAF_IMPORT result=FAIL stage=open_input_stream";
                try (FileOutputStream output = new FileOutputStream(temp, false)) {
                    int count;
                    while ((count = input.read(buffer)) != -1) {
                        if (count == 0) continue;
                        long freeNow = importDir.getUsableSpace();
                        if (freeNow < count + reserveBytes)
                            throw new java.io.IOException("insufficient free space during copy");
                        output.write(buffer, 0, count);
                        digest.update(buffer, 0, count);
                        total += count;
                    }
                    output.flush();
                    output.getFD().sync();
                }
            }
            if (reportedSize >= 0 && total != reportedSize)
                throw new java.io.IOException("size mismatch expected=" + reportedSize + " copied=" + total);

            try {
                Files.move(temp.toPath(), destination.toPath(), StandardCopyOption.ATOMIC_MOVE);
            } catch (AtomicMoveNotSupportedException unsupported) {
                if (!temp.renameTo(destination))
                    throw new java.io.IOException("atomic publish failed");
            }
            temp = null;
            StringBuilder hex = new StringBuilder(64);
            for (byte b : digest.digest()) hex.append(String.format(Locale.ROOT, "%02x", b & 0xff));
            String mime = context.getContentResolver().getType(uri);
            return "SAF_IMPORT result=PASS bytes=" + total
                    + " sha256=" + hex
                    + " mime=" + (mime == null ? "unknown" : mime)
                    + " stored=" + destination.getAbsolutePath()
                    + " source_uri_used_by_native=no";
        } catch (Throwable error) {
            return "SAF_IMPORT result=FAIL error=" + error.getClass().getSimpleName()
                    + " message=" + String.valueOf(error.getMessage());
        } finally {
            if (temp != null && temp.exists()) temp.delete();
        }
    }

    private static String safeFileName(String name) {
        if (name == null || name.trim().isEmpty()) name = "document.bin";
        String safe = name.replace('/', '_').replace('\\', '_');
        safe = safe.replaceAll("\\p{Cntrl}", "_").trim();
        while (safe.startsWith(".")) safe = safe.substring(1);
        if (safe.isEmpty()) safe = "document.bin";
        if (safe.length() > 120) safe = safe.substring(safe.length() - 120);
        return safe;
    }
}
