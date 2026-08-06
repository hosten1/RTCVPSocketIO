package com.lymvpsocketio.demo;

import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;

import com.lymvpsocketio.SocketIOClient;

import org.json.JSONArray;

public class MainActivity extends AppCompatActivity {

    private static final String TAG = "SocketIODemo";

    private SocketIOClient socketClient;
    private EditText etUrl;
    private EditText etEvent;
    private EditText etMessage;
    private TextView tvStatus;
    private TextView tvSid;
    private TextView tvLog;
    private Button btnConnect;
    private Button btnDisconnect;
    private Button btnSend;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        etUrl = findViewById(R.id.etUrl);
        etEvent = findViewById(R.id.etEvent);
        etMessage = findViewById(R.id.etMessage);
        tvStatus = findViewById(R.id.tvStatus);
        tvSid = findViewById(R.id.tvSid);
        tvLog = findViewById(R.id.tvLog);
        btnConnect = findViewById(R.id.btnConnect);
        btnDisconnect = findViewById(R.id.btnDisconnect);
        btnSend = findViewById(R.id.btnSend);

        try {
            socketClient = new SocketIOClient();
            log("Native library loaded successfully");
            log("SocketIOClient created");
        } catch (UnsatisfiedLinkError e) {
            log("ERROR: Failed to load native library: " + e.getMessage());
            Toast.makeText(this, "Failed to load native library", Toast.LENGTH_LONG).show();
        }

        setupCallbacks();
    }

    private void setupCallbacks() {
        if (socketClient == null) return;

        socketClient.setConnectCallback(new SocketIOClient.ConnectCallback() {
            @Override
            public void onConnected() {
                runOnUiThread(() -> {
                    tvStatus.setText("Status: Connected");
                    tvSid.setText("SID: " + socketClient.getSid());
                    btnConnect.setEnabled(false);
                    btnDisconnect.setEnabled(true);
                    btnSend.setEnabled(true);
                    log("CONNECTED");
                });
            }

            @Override
            public void onDisconnected(String reason) {
                runOnUiThread(() -> {
                    tvStatus.setText("Status: Disconnected");
                    tvSid.setText("SID: -");
                    btnConnect.setEnabled(true);
                    btnDisconnect.setEnabled(false);
                    btnSend.setEnabled(false);
                    log("DISCONNECTED: " + reason);
                });
            }

            @Override
            public void onError(String error) {
                runOnUiThread(() -> {
                    log("ERROR: " + error);
                });
            }
        });

        socketClient.setEventCallback(new SocketIOClient.EventCallback() {
            @Override
            public void onEvent(String event, JSONArray args) {
                runOnUiThread(() -> {
                    log("EVENT: " + event + " -> " + (args != null ? args.toString() : "null"));
                });
            }
        });
    }

    public void onConnectClick(View view) {
        String url = etUrl.getText().toString().trim();
        if (url.isEmpty()) {
            Toast.makeText(this, "Please enter server URL", Toast.LENGTH_SHORT).show();
            return;
        }

        log("Connecting to: " + url);
        new Thread(() -> {
            try {
                socketClient.setVersion(SocketIOClient.SocketIOVersion.V4);
                socketClient.setTransport(SocketIOClient.TransportType.WEBSOCKET);
                socketClient.connect(url);
            } catch (Exception e) {
                runOnUiThread(() -> log("Connect error: " + e.getMessage()));
            }
        }).start();
    }

    public void onDisconnectClick(View view) {
        log("Disconnecting...");
        new Thread(() -> {
            try {
                socketClient.disconnect();
            } catch (Exception e) {
                runOnUiThread(() -> log("Disconnect error: " + e.getMessage()));
            }
        }).start();
    }

    public void onSendClick(View view) {
        String event = etEvent.getText().toString().trim();
        String message = etMessage.getText().toString().trim();

        if (event.isEmpty()) {
            Toast.makeText(this, "Please enter event name", Toast.LENGTH_SHORT).show();
            return;
        }

        log("Sending: " + event + " -> " + message);
        new Thread(() -> {
            try {
                JSONArray args = new JSONArray();
                args.put(message);
                socketClient.emit(event, args);
                runOnUiThread(() -> log("Message sent"));
            } catch (Exception e) {
                runOnUiThread(() -> log("Send error: " + e.getMessage()));
            }
        }).start();
    }

    private void log(String message) {
        Log.d(TAG, message);
        String current = tvLog.getText().toString();
        tvLog.setText(current + message + "\n");
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (socketClient != null) {
            try {
                socketClient.release();
            } catch (Exception e) {
                Log.e(TAG, "Release error", e);
            }
        }
    }
}
