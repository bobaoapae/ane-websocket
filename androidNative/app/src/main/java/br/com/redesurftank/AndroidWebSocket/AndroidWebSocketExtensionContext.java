package br.com.redesurftank.AndroidWebSocket;

import android.os.Build;
import android.webkit.WebSettings;

import androidx.annotation.RequiresApi;

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
import java.net.InetSocketAddress;
import java.net.Socket;
import java.net.SocketAddress;
import java.net.URI;
import java.net.UnknownHostException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.Queue;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class AndroidWebSocketExtensionContext extends FREContext {

    private static final String CTX_NAME = "AndroidWebSocketExtensionContext";
    private String tag;
    private AndroidWebSocket _socket;
    private Queue<byte[]> _byteBufferQueue;
    private Map<String, List<String>> _staticHosts;

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
        functionMap.put(AddStaticHost.KEY, new AddStaticHost());
        functionMap.put(RemoveStaticHost.KEY, new RemoveStaticHost());
        return functionMap;

    }

    @Override
    public void dispose() {
        AndroidWebSocketLogger.i(this.tag, "Dispose context");
    }

    public static class ConnectFunction implements FREFunction {
        public static final String KEY = "connect";
        private static final String TAG = "AndroidWebSocketConnect";
        private static final ExecutorService executor = Executors.newCachedThreadPool();


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
                        List<InetAddress> addresses = new ArrayList<>();

                        int port = uri.getPort();
                        if (port == -1) {
                            port = uri.getScheme().equals("wss") ? 443 : 80;
                        }

                        try {
                            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.N) {
                                addresses.addAll(resolveDnsUsingThreadForLowApi(uri.getHost()));
                            } else {
                                List<InetAddress> fromResolversResult = resolveWithDns(uri.getHost()).join();
                                addresses.addAll(fromResolversResult);
                            }
                        } catch (Exception e) {
                            AndroidWebSocketLogger.e(TAG, "Error in lookup() : " + e.getMessage(), e);
                        }

                        if (addresses.isEmpty()) {
                            synchronized (context._staticHosts) {
                                if (context._staticHosts.containsKey(uri.getHost())) {
                                    for (String ip : context._staticHosts.get(uri.getHost())) {
                                        try {
                                            addresses.add(InetAddress.getByName(ip));
                                        } catch (UnknownHostException e) {
                                            AndroidWebSocketLogger.e(TAG, "Failure in resolve() : " + e.getMessage(), e);
                                        }
                                    }
                                }
                            }
                        }

                        if (!addresses.isEmpty()) {
                            for (InetAddress address : addresses) {
                                SocketAddress socketAddress = new InetSocketAddress(address, port);
                                try (Socket socket = new Socket()) {
                                    socket.connect(socketAddress, 1000);
                                    if (socket.isConnected()) {
                                        return address;
                                    }
                                } catch (IOException ignored) {

                                }
                            }
                        }

                        //check if the address is reachable and return
                        for (InetAddress address : InetAddress.getAllByName(uri.getHost())) {
                            try (Socket socket = new Socket()) {
                                socket.connect(new InetSocketAddress(address, port), 1000);
                                if (socket.isConnected()) {
                                    return address;
                                }
                            } catch (IOException ignored) {

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

        private InetAddress getByIpWithoutException(String ip) {
            try {
                return InetAddress.getByName(ip);
            } catch (UnknownHostException e) {
                AndroidWebSocketLogger.e(TAG, "Failure in getByIpWithoutException() : " + e.getMessage(), e);
                return null;
            }
        }

        private List<InetAddress> resolveDnsUsingThreadForLowApi(String domain) {
            AndroidWebSocketLogger.d(TAG, "resolveDnsUsingThreadForLowApi() called with: domain = [" + domain + "]");
            List<InetAddress> addresses = new ArrayList<>();
            Thread cloudflareThread = new Thread(() -> {
                try {
                    List<InetAddress> cloudflareAddresses = resolveDns(new DohResolver("https://1.1.1.1/dns-query"), domain);
                    synchronized (addresses) {
                        if (!addresses.isEmpty()) return;
                        AndroidWebSocketLogger.d(TAG, "resolveDnsUsingThreadForLowApi() resolved with cloudflare");
                        addresses.addAll(cloudflareAddresses);
                    }
                } catch (Exception e) {
                    AndroidWebSocketLogger.e(TAG, "Failure in resolveDnsUsingThreadForLowApi() : " + e.getMessage(), e);
                }
            });
            Thread googleThread = new Thread(() -> {
                try {
                    List<InetAddress> googleAddresses = resolveDns(new DohResolver("https://dns.google/dns-query"), domain);
                    synchronized (addresses) {
                        if (!addresses.isEmpty()) return;
                        AndroidWebSocketLogger.d(TAG, "resolveDnsUsingThreadForLowApi() resolved with google");
                        addresses.addAll(googleAddresses);
                    }
                } catch (Exception e) {
                    AndroidWebSocketLogger.e(TAG, "Failure in resolveDnsUsingThreadForLowApi() : " + e.getMessage(), e);
                }
            });
            Thread adguardThread = new Thread(() -> {
                try {
                    List<InetAddress> adguardAddresses = resolveDns(new DohResolver("https://unfiltered.adguard-dns.com/dns-query"), domain);
                    synchronized (addresses) {
                        if (!addresses.isEmpty()) return;
                        AndroidWebSocketLogger.d(TAG, "resolveDnsUsingThreadForLowApi() resolved with adguard");
                        addresses.addAll(adguardAddresses);
                    }
                } catch (Exception e) {
                    AndroidWebSocketLogger.e(TAG, "Failure in resolveDnsUsingThreadForLowApi() : " + e.getMessage(), e);
                }
            });
            Thread cloudflareNormalThread = new Thread(() -> {
                try {
                    List<InetAddress> adguardAddresses = resolveDns(new SimpleResolver(Objects.requireNonNull(getByIpWithoutException("1.1.1.1"))), domain);
                    synchronized (addresses) {
                        if (!addresses.isEmpty()) return;
                        AndroidWebSocketLogger.d(TAG, "resolveDnsUsingThreadForLowApi() resolved with cloudflare normal");
                        addresses.addAll(adguardAddresses);
                    }
                } catch (Exception e) {
                    AndroidWebSocketLogger.e(TAG, "Failure in resolveDnsUsingThreadForLowApi() : " + e.getMessage(), e);
                }
            });
            Thread googleNormalThread = new Thread(() -> {
                try {
                    List<InetAddress> adguardAddresses = resolveDns(new SimpleResolver(Objects.requireNonNull(getByIpWithoutException("8.8.8.8"))), domain);
                    synchronized (addresses) {
                        if (!addresses.isEmpty()) return;
                        AndroidWebSocketLogger.d(TAG, "resolveDnsUsingThreadForLowApi() resolved with google normal");
                        addresses.addAll(adguardAddresses);
                    }
                } catch (Exception e) {
                    AndroidWebSocketLogger.e(TAG, "Failure in resolveDnsUsingThreadForLowApi() : " + e.getMessage(), e);
                }
            });
            Thread adGuardNormalThread = new Thread(() -> {
                try {
                    List<InetAddress> adguardAddresses = resolveDns(new SimpleResolver(Objects.requireNonNull(getByIpWithoutException("94.140.14.140"))), domain);
                    synchronized (addresses) {
                        if (!addresses.isEmpty()) return;
                        AndroidWebSocketLogger.d(TAG, "resolveDnsUsingThreadForLowApi() resolved with adguard normal");
                        addresses.addAll(adguardAddresses);
                    }
                } catch (Exception e) {
                    AndroidWebSocketLogger.e(TAG, "Failure in resolveDnsUsingThreadForLowApi() : " + e.getMessage(), e);
                }
            });

            cloudflareThread.start();
            googleThread.start();
            adguardThread.start();
            cloudflareNormalThread.start();
            googleNormalThread.start();
            adGuardNormalThread.start();

            while (true) {
                synchronized (addresses) {
                    if (!addresses.isEmpty()) {
                        break;
                    }
                }
                //check if all threads are done
                if (!cloudflareThread.isAlive() && !googleThread.isAlive() && !adguardThread.isAlive() && !cloudflareNormalThread.isAlive() && !googleNormalThread.isAlive() && !adGuardNormalThread.isAlive()) {
                    break;
                }
                try {
                    Thread.sleep(100);
                } catch (InterruptedException e) {
                    throw new RuntimeException(e);
                }
            }

            return addresses;
        }

        @RequiresApi(api = Build.VERSION_CODES.N)
        private CompletableFuture<List<InetAddress>> resolveWithDns(String domain) {
            CompletableFuture<List<InetAddress>> cloudflareFuture = CompletableFuture.supplyAsync(() -> resolveDns(new DohResolver("https://1.1.1.1/dns-query"), domain), executor);
            CompletableFuture<List<InetAddress>> googleFuture = CompletableFuture.supplyAsync(() -> resolveDns(new DohResolver("https://dns.google/dns-query"), domain), executor);
            CompletableFuture<List<InetAddress>> adguardFuture = CompletableFuture.supplyAsync(() -> resolveDns(new DohResolver("https://unfiltered.adguard-dns.com/dns-query"), domain), executor);
            CompletableFuture<List<InetAddress>> cloudflareNormalFuture = CompletableFuture.supplyAsync(() -> resolveDns(new SimpleResolver(Objects.requireNonNull(getByIpWithoutException("1.1.1.1"))), domain), executor);
            CompletableFuture<List<InetAddress>> googleNormalFuture = CompletableFuture.supplyAsync(() -> resolveDns(new SimpleResolver(Objects.requireNonNull(getByIpWithoutException("8.8.8.8"))), domain), executor);
            CompletableFuture<List<InetAddress>> adguardNormalFuture = CompletableFuture.supplyAsync(() -> resolveDns(new SimpleResolver(Objects.requireNonNull(getByIpWithoutException("94.140.14.140"))), domain), executor);

            return CompletableFuture.anyOf(cloudflareFuture, googleFuture, adguardFuture, cloudflareNormalFuture, googleNormalFuture, adguardNormalFuture).thenApply(o -> {
                if (cloudflareFuture.isDone() && !cloudflareFuture.isCompletedExceptionally()) {
                    AndroidWebSocketLogger.d(TAG, "resolveWithDns() resolved with cloudflare");
                    return cloudflareFuture.join();
                } else if (googleFuture.isDone() && !googleFuture.isCompletedExceptionally()) {
                    AndroidWebSocketLogger.d(TAG, "resolveWithDns() resolved with google");
                    return googleFuture.join();
                } else if (adguardFuture.isDone() && !adguardFuture.isCompletedExceptionally()) {
                    AndroidWebSocketLogger.d(TAG, "resolveWithDns() resolved with adguard");
                    return adguardFuture.join();
                } else if (cloudflareNormalFuture.isDone() && !cloudflareNormalFuture.isCompletedExceptionally()) {
                    AndroidWebSocketLogger.d(TAG, "resolveWithDns() resolved with cloudflare normal");
                    return cloudflareNormalFuture.join();
                } else if (googleNormalFuture.isDone() && !googleNormalFuture.isCompletedExceptionally()) {
                    AndroidWebSocketLogger.d(TAG, "resolveWithDns() resolved with google normal");
                    return googleNormalFuture.join();
                } else if (adguardNormalFuture.isDone() && !adguardNormalFuture.isCompletedExceptionally()) {
                    AndroidWebSocketLogger.d(TAG, "resolveWithDns() resolved with adguard normal");
                    return adguardNormalFuture.join();
                }

                return new ArrayList<>();
            });
        }

        private List<InetAddress> resolveDns(Resolver resolver, String domain) {
            try {
                Record queryRecord = Record.newRecord(Name.fromString(domain + "."), Type.A, DClass.IN);
                Message queryMessage = Message.newQuery(queryRecord);
                Message result = resolver.send(queryMessage);
                List<Record> answers = result.getSection(Section.ANSWER);
                List<InetAddress> addresses = new ArrayList<>();
                for (Record record : answers) {
                    if (record.getType() == Type.A || record.getType() == Type.AAAA) {
                        addresses.add(InetAddress.getByName(record.rdataToString()));
                    }
                }
                return addresses;
            } catch (Exception e) {
                AndroidWebSocketLogger.d(TAG, "Failure in resolveDns() : " + e.getMessage(), e);
                throw new RuntimeException(e);
            }
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

    public static class AddStaticHost implements FREFunction {
        public static final String KEY = "addStaticHost";
        private static final String TAG = "AndroidWebSocketAddStaticHost";

        @Override
        public FREObject call(FREContext freContext, FREObject[] freObjects) {
            try {
                AndroidWebSocketLogger.i(TAG, "Adding static host");
                AndroidWebSocketExtensionContext context = (AndroidWebSocketExtensionContext) freContext;

                String host = freObjects[0].getAsString();
                String ip = freObjects[1].getAsString();

                synchronized (context._staticHosts) {
                    if (!context._staticHosts.containsKey(host)) {
                        context._staticHosts.put(host, new ArrayList<>());
                    }

                    context._staticHosts.get(host).add(ip);
                }

                return FREObject.newObject(true);
            } catch (Exception e) {
                AndroidWebSocketLogger.e(TAG, "Error adding static host", e);
            }

            return null;
        }
    }

    public static class RemoveStaticHost implements FREFunction {
        public static final String KEY = "removeStaticHost";
        private static final String TAG = "AndroidWebSocketRemoveStaticHost";

        @Override
        public FREObject call(FREContext freContext, FREObject[] freObjects) {
            try {
                AndroidWebSocketLogger.i(TAG, "Removing static host");
                AndroidWebSocketExtensionContext context = (AndroidWebSocketExtensionContext) freContext;

                String host = freObjects[0].getAsString();

                synchronized (context._staticHosts) {
                    context._staticHosts.remove(host);
                }

                return FREObject.newObject(true);
            } catch (Exception e) {
                AndroidWebSocketLogger.e(TAG, "Error removing static host", e);
            }

            return null;
        }
    }
}
