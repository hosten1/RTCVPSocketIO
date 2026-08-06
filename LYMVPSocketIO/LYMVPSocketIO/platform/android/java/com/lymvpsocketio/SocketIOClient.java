package com.lymvpsocketio;

import org.json.JSONArray;

public class SocketIOClient {
    static {
        System.loadLibrary("lymvpsocketio");
    }

    public enum SocketIOVersion {
        V2(2),
        V3(3),
        V4(4);

        private final int value;
        SocketIOVersion(int value) { this.value = value; }
        public int getValue() { return value; }
    }

    public enum TransportType {
        POLLING(0),
        WEBSOCKET(1);

        private final int value;
        TransportType(int value) { this.value = value; }
        public int getValue() { return value; }
    }

    public interface ConnectCallback {
        void onConnected();
        void onDisconnected(String reason);
        void onError(String error);
    }

    public interface EventCallback {
        void onEvent(String event, JSONArray args);
    }

    public interface AckCallback {
        void onAck(JSONArray args);
        void onTimeout();
    }

    private long nativeHandle;
    private ConnectCallback connectCallback;
    private EventCallback eventCallback;

    public SocketIOClient() {
        nativeHandle = nativeCreate();
    }

    public void setVersion(SocketIOVersion version) {
        nativeSetVersion(version.getValue());
    }

    public void setTransport(TransportType transport) {
        nativeSetTransport(transport.getValue());
    }

    public void setSelfSignedSSL(boolean enabled) {
        nativeSetSelfSignedSSL(enabled);
    }

    public void setConnectCallback(ConnectCallback callback) {
        this.connectCallback = callback;
    }

    public void setEventCallback(EventCallback callback) {
        this.eventCallback = callback;
    }

    public void connect(String url) {
        nativeConnect(url);
    }

    public void disconnect() {
        nativeDisconnect();
    }

    public boolean isConnected() {
        return nativeIsConnected();
    }

    public String getSid() {
        return nativeGetSid();
    }

    public void emit(String event, JSONArray args) {
        nativeEmit(event, args != null ? args.toString() : "[]");
    }

    public void emitWithAck(String event, JSONArray args, AckCallback callback, int timeoutMs) {
        int ackId = nativeEmitWithAck(event, args != null ? args.toString() : "[]", timeoutMs);
        pendingAcks.put(ackId, callback);
    }

    public void on(String eventName) {
        nativeOn(eventName);
    }

    public void off(String eventName) {
        nativeOff(eventName);
    }

    public void release() {
        if (nativeHandle != 0) {
            nativeRelease(nativeHandle);
            nativeHandle = 0;
        }
    }

    @Override
    protected void finalize() throws Throwable {
        release();
        super.finalize();
    }

    private static java.util.Map<Integer, AckCallback> pendingAcks =
        new java.util.concurrent.ConcurrentHashMap<>();

    private static int nextAckId = 0;

    private native long nativeCreate();
    private native void nativeSetVersion(int version);
    private native void nativeSetTransport(int transport);
    private native void nativeSetSelfSignedSSL(boolean enabled);
    private native void nativeConnect(String url);
    private native void nativeDisconnect();
    private native boolean nativeIsConnected();
    private native String nativeGetSid();
    private native void nativeEmit(String event, String argsJson);
    private native int nativeEmitWithAck(String event, String argsJson, int timeoutMs);
    private native void nativeOn(String eventName);
    private native void nativeOff(String eventName);
    private native void nativeRelease(long handle);

    private void onNativeConnected() {
        if (connectCallback != null) {
            connectCallback.onConnected();
        }
    }

    private void onNativeDisconnected(String reason) {
        if (connectCallback != null) {
            connectCallback.onDisconnected(reason);
        }
    }

    private void onNativeError(String error) {
        if (connectCallback != null) {
            connectCallback.onError(error);
        }
    }

    private void onNativeEvent(String event, String argsJson) {
        if (eventCallback != null) {
            try {
                JSONArray args = new JSONArray(argsJson);
                eventCallback.onEvent(event, args);
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
    }

    private void onNativeAck(int ackId, String argsJson) {
        AckCallback cb = pendingAcks.remove(ackId);
        if (cb != null) {
            try {
                JSONArray args = new JSONArray(argsJson);
                cb.onAck(args);
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
    }

    private void onNativeAckTimeout(int ackId) {
        AckCallback cb = pendingAcks.remove(ackId);
        if (cb != null) {
            cb.onTimeout();
        }
    }
}
