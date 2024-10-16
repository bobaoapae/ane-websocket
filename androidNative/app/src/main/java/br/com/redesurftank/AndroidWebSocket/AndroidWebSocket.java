package br.com.redesurftank.AndroidWebSocket;

import android.net.TrafficStats;

import org.java_websocket.client.WebSocketClient;
import org.java_websocket.drafts.Draft;
import org.java_websocket.handshake.ServerHandshake;

import java.net.URI;
import java.nio.ByteBuffer;
import java.util.Map;

public class AndroidWebSocket extends WebSocketClient {

    private static final String TAG = "AndroidWebSocket";

    private AndroidWebSocketExtensionContext _context;

    public AndroidWebSocket(URI serverUri, AndroidWebSocketExtensionContext _context) {
        super(serverUri);
        this._context = _context;
    }

    public AndroidWebSocket(URI serverUri, Draft protocolDraft, AndroidWebSocketExtensionContext _context) {
        super(serverUri, protocolDraft);
        this._context = _context;
    }

    public AndroidWebSocket(URI serverUri, Map<String, String> httpHeaders, AndroidWebSocketExtensionContext _context) {
        super(serverUri, httpHeaders);
        this._context = _context;
    }

    public AndroidWebSocket(URI serverUri, Draft protocolDraft, Map<String, String> httpHeaders, AndroidWebSocketExtensionContext _context) {
        super(serverUri, protocolDraft, httpHeaders);
        this._context = _context;
    }

    public AndroidWebSocket(URI serverUri, Draft protocolDraft, Map<String, String> httpHeaders, int connectTimeout, AndroidWebSocketExtensionContext _context) {
        super(serverUri, protocolDraft, httpHeaders, connectTimeout);
        this._context = _context;
    }

    private String encode(byte[] data) {
        StringBuilder hexString = new StringBuilder();
        for (byte b : data) {
            String hex = Integer.toHexString(0xFF & b);
            if (hex.length() == 1) {
                hexString.append('0');
            }
            hexString.append(hex);
        }
        return hexString.toString();
    }

    @Override
    public void run() {
        TrafficStats.setThreadStatsTag((int) Thread.currentThread().getId());
        super.run();
    }

    @Override
    public void onOpen(ServerHandshake handshakedata) {
        AndroidWebSocketLogger.d(TAG, "Callback: onConnected");
        dispatchStatusEventAsync("connected", "");

    }

    @Override
    public void onMessage(String message) {
        AndroidWebSocketLogger.d(TAG, "Callback: onTextMessage");
        dispatchStatusEventAsync("textMessage", message);
    }

    @Override
    public void onMessage(ByteBuffer bytes) {
        AndroidWebSocketLogger.d(TAG, "Callback: onBinaryMessage");
        this._context.addByteBuffer(bytes.array());
        dispatchStatusEventAsync("nextMessage", "");
    }

    @Override
    public void onClose(int code, String reason, boolean remote) {
        AndroidWebSocketLogger.d(TAG, "Callback: onDisconnected " + code + " " + reason + " " + remote);
        dispatchStatusEventAsync("disconnected", code + ";" + reason + ";" + remote);
        this._context = null;
    }

    @Override
    public void onError(Exception ex) {
        AndroidWebSocketLogger.e(TAG, "Callback: onError ", ex);
        String message = ex.getMessage();
        if (message == null) {
            message = "Unknown error";
        }
        StringBuilder stackTrace = new StringBuilder();
        for (StackTraceElement ste : ex.getStackTrace()) {
            stackTrace.append(ste.toString()).append("\n");
        }
        dispatchStatusEventAsync("error", message + "\n" + stackTrace);
        this._context = null;
    }

    private void dispatchStatusEventAsync(String s, String s1) {
        if (this._context == null) {
            AndroidWebSocketLogger.e(TAG, "Context is null");
            return;
        }

        try {
            this._context.dispatchStatusEventAsync(s, s1);
        } catch (Exception e) {
            AndroidWebSocketLogger.e(TAG, "Error dispatching event", e);
        }
    }
}
