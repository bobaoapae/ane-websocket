package br.com.redesurftank.AndroidWebSocket;

import android.util.Log;
import android.webkit.WebSettings;

import com.adobe.fre.FREByteArray;
import com.adobe.fre.FREContext;
import com.adobe.fre.FREFunction;
import com.adobe.fre.FREObject;

import org.java_websocket.client.DnsResolver;
import org.java_websocket.drafts.Draft_6455;
import org.xbill.DNS.DClass;
import org.xbill.DNS.DohResolver;
import org.xbill.DNS.Message;
import org.xbill.DNS.Name;
import org.xbill.DNS.Record;
import org.xbill.DNS.Resolver;
import org.xbill.DNS.Section;
import org.xbill.DNS.SimpleResolver;
import org.xbill.DNS.Type;

import java.io.IOException;
import java.net.InetAddress;
import java.net.URI;
import java.net.UnknownHostException;
import java.nio.ByteBuffer;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Queue;
import java.util.UUID;
import java.util.concurrent.ConcurrentLinkedQueue;

public class AndroidWebSocketExtensionContext extends FREContext {

    private static final String CTX_NAME = "AndroidWebSocketExtensionContext";
    private String tag;
    private AndroidWebSocket _socket;
    private Queue<byte[]> _byteBufferQueue;

    public AndroidWebSocketExtensionContext(String extensionName) {
        this.tag = extensionName + "." + CTX_NAME;
        AndroidWebSocketLogger.i(this.tag, "Creating context");
        _byteBufferQueue = new ConcurrentLinkedQueue<>();
    }

    public String getIdentifier() {
        return this.tag;
    }

    public boolean addByteBuffer(byte[] byteBuffer) {
        return _byteBufferQueue.add(byteBuffer);
    }

    @Override
    public Map<String, FREFunction> getFunctions() {
        AndroidWebSocketLogger.i(this.tag, "Creating function Map");
        Map<String, FREFunction> functionMap = new HashMap<>();
        functionMap.put(SetDebugMode.KEY, new SetDebugMode());
        functionMap.put(ConnectFunction.KEY, new ConnectFunction());
        functionMap.put(SendMessageFunction.KEY, new SendMessageFunction());
        functionMap.put(CloseFunction.KEY, new CloseFunction());
        functionMap.put(GetByteArrayMessage.KEY, new GetByteArrayMessage());
        return functionMap;

    }

    @Override
    public void dispose() {
        AndroidWebSocketLogger.i(this.tag, "Dispose context");
    }

    public static class ConnectFunction implements FREFunction {
        public static final String KEY = "connect";
        private static final String TAG = "AndroidWebSocketConnect";

        @Override
        public FREObject call(FREContext freContext, FREObject[] freObjects) {
            AndroidWebSocketLogger.d(TAG, "Called connect");
            boolean success = false;
            FREObject retVal = null;
            AndroidWebSocketExtensionContext context = (AndroidWebSocketExtensionContext) freContext;
            try {
                if (context._socket != null) {
                    context._socket.close();
                }
            } catch (Exception e) {
                AndroidWebSocketLogger.e(TAG, "Failure in close() method: " + e.getMessage());
            }
            try {
                String url = freObjects[0].getAsString();
                Map<String, String> headers = new HashMap<>();
                // add default user agent from the context
                String defaultWebViewUserAgent = WebSettings.getDefaultUserAgent(context.getActivity());
                String appPackageName = context.getActivity().getPackageName();
                String appVersion = context.getActivity().getPackageManager().getPackageInfo(appPackageName, 0).versionName;
                headers.put("User-Agent", defaultWebViewUserAgent + " " + appPackageName + "/" + appVersion);
                context._socket = new AndroidWebSocket(URI.create(url), new Draft_6455(), headers, 5000, context);
                context._socket.setDnsResolver(new DnsResolver() {
                    @Override
                    public InetAddress resolve(URI uri) throws UnknownHostException {
                        try {
                            Log.d(TAG, "Resolving address using cloudflare DoH");
                            DohResolver dohResolver = new DohResolver("https://1.1.1.1/dns-query");
                            Record queryRecord = Record.newRecord(Name.fromString(uri.getHost() + "."), Type.A, DClass.IN);
                            Message queryMessage = Message.newQuery(queryRecord);
                            Message result = dohResolver.send(queryMessage);
                            List<Record> answers = result.getSection(Section.ANSWER);
                            for (Record record : answers) {
                                if (record.getType() == Type.A) {
                                    String address = record.rdataToString();
                                    Log.d(TAG, "Resolved address: " + address);
                                    InetAddress inetAddress = InetAddress.getByName(address);
                                    if (inetAddress.isReachable(1000)) {
                                        Log.d(TAG, "Address is reachable");
                                        return inetAddress;
                                    }
                                }
                            }

                        } catch (Exception e) {
                            AndroidWebSocketLogger.e(TAG, "Failure in resolve() method using doh cloudflare: " + e.getMessage(), e);
                        }

                        try {
                            Log.d(TAG, "Resolving address using cloudflare dns normal udp");
                            Resolver resolver = new SimpleResolver(InetAddress.getByName("1.1.1.1"));
                            Record queryRecord = Record.newRecord(Name.fromString(uri.getHost() + "."), Type.A, DClass.IN);
                            Message queryMessage = Message.newQuery(queryRecord);
                            Message result = resolver.send(queryMessage);
                            List<Record> answers = result.getSection(Section.ANSWER);
                            for (Record record : answers) {
                                if (record.getType() == Type.A) {
                                    String address = record.rdataToString();
                                    Log.d(TAG, "Resolved address: " + address);
                                    InetAddress inetAddress = InetAddress.getByName(address);
                                    if (inetAddress.isReachable(1000)) {
                                        Log.d(TAG, "Address is reachable");
                                        return inetAddress;
                                    }
                                }
                            }
                        } catch (Exception e) {
                            AndroidWebSocketLogger.e(TAG, "Failure in resolve() method using udp cloudflare: " + e.getMessage(), e);
                        }

                        InetAddress[] addresses = InetAddress.getAllByName(uri.getHost());
                        //check if the address is reachable and return
                        for (InetAddress address : addresses) {
                            try {
                                if (address.isReachable(1000)) {
                                    return address;
                                }
                            } catch (IOException e) {
                            }
                        }
                        throw new UnknownHostException("Could not resolve address");
                    }
                });
                context._socket.connect();
            } catch (Exception e) {
                AndroidWebSocketLogger.e(TAG, "Failure in connect() method: " + e.getMessage());
            }
            try {
                retVal = FREObject.newObject(success);
            } catch (Exception e2) {
                AndroidWebSocketLogger.e(TAG, "Could not create return value: " + e2.getMessage());
            }
            return retVal;

        }
    }

    public static class SetDebugMode implements FREFunction {
        public static final String KEY = "SetDebugMode";
        private static final String TAG = "SetDebugMode";

        public FREObject call(FREContext freContext, FREObject[] args) {
            FREObject returnValue = null;
            AndroidWebSocketLogger.i("SetDebugMode", "SetDebugMode");
            try {
                FREObject input = args[0];
                AndroidWebSocketLogger.g_enableReleaseLogging = input.getAsBool();
                returnValue = FREObject.newObject(AndroidWebSocketLogger.g_enableReleaseLogging);
            } catch (Exception e) {
                AndroidWebSocketLogger.e("SetDebugMode", "Exception", e);
            }
            AndroidWebSocketLogger.i("SetDebugMode", "SetDebugMode end");
            return returnValue;
        }
    }

    public static class SendMessageFunction implements FREFunction {
        public static final String KEY = "sendMessage";
        private static final String TAG = "AndroidWebSocketSendMessage";

        public FREObject call(FREContext freContext, FREObject[] freObjects) {
            AndroidWebSocketLogger.d(TAG, "Called sendMessage");
            boolean success = false;
            FREObject retVal = null;
            try {
                AndroidWebSocket client = ((AndroidWebSocketExtensionContext) freContext)._socket;
                if (client.isOpen()) {
                    int opCode = freObjects[0].getAsInt();
                    if (freObjects[1] instanceof FREByteArray) {
                        AndroidWebSocketLogger.d(TAG, "Message is a byte array");
                        FREByteArray byteArray = (FREByteArray) freObjects[1];
                        byteArray.acquire();
                        client.send(byteArray.getBytes());
                        byteArray.release();
                        success = true;
                    } else if (freObjects[1] != null) {
                        AndroidWebSocketLogger.d(TAG, "Message is string");
                        String strMessage = freObjects[1].getAsString();
                        client.send(strMessage);
                        success = true;
                    } else {
                        AndroidWebSocketLogger.e(TAG, "Message is null");
                    }
                } else {
                    AndroidWebSocketLogger.d(TAG, "Socket is not open");
                }
            } catch (Exception e) {
                AndroidWebSocketLogger.e(TAG, "Failure in sendMessage() method: ", e);
            }
            try {
                retVal = FREObject.newObject(success);
            } catch (Exception e2) {
                AndroidWebSocketLogger.e(TAG, "Could not create return value: ", e2);
            }
            return retVal;
        }
    }

    public static class CloseFunction implements FREFunction {
        public static final String KEY = "close";
        private static final String TAG = "AndroidWebSocketClose";

        public FREObject call(FREContext freContext, FREObject[] freObjects) {
            AndroidWebSocketLogger.d(TAG, "Called close");
            boolean success = false;
            FREObject retVal = null;
            try {
                AndroidWebSocket client = ((AndroidWebSocketExtensionContext) freContext)._socket;
                int closeReason = freObjects[0].getAsInt();
                client.close(closeReason);
                success = true;
            } catch (Exception e) {
                AndroidWebSocketLogger.e(TAG, "Failure in close() method: ", e);
            }
            try {
                retVal = FREObject.newObject(success);
            } catch (Exception e2) {
                AndroidWebSocketLogger.e(TAG, "Could not create return value: ", e2);
            }
            return retVal;
        }
    }

    public static class GetByteArrayMessage implements FREFunction {
        public static final String KEY = "getByteArrayMessage";
        private static final String TAG = "AndroidWebSocketGetByteArrayMessage";

        @Override
        public FREObject call(FREContext freContext, FREObject[] freObjects) {
            AndroidWebSocketLogger.d(TAG, "Called getByteArrayMessage");
            try {
                AndroidWebSocketExtensionContext context = (AndroidWebSocketExtensionContext) freContext;
                byte[] byteBuffer = context._byteBufferQueue.poll();
                if (byteBuffer == null) {
                    return null;
                }
                FREByteArray array = FREByteArray.newByteArray(byteBuffer.length);
                array.acquire();
                array.getBytes().put(byteBuffer);
                array.release();
                return array;
            } catch (Exception e) {
                AndroidWebSocketLogger.e(TAG, "Failure in getByteArrayMessage() method: ", e);
            }
            return null;
        }
    }
}
