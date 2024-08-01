package br.com.redesurftank.AndroidWebSocket;


import com.adobe.fre.FREContext;
import com.adobe.fre.FREExtension;

public class AndroidWebSocketExtension implements FREExtension {

    private static final String EXT_NAME = "AndroidWebSocketExtension";
    private AndroidWebSocketExtensionContext context;
    private String tag = "AndroidWebSocketExtensionAndroidWebSocketExtensionClass";

    @Override
    public FREContext createContext(String arg0) {
        AndroidWebSocketLogger.i(this.tag, "Creating context");
        this.context = new AndroidWebSocketExtensionContext(EXT_NAME);
        return this.context;
    }

    @Override
    public void dispose() {
        AndroidWebSocketLogger.i(this.tag, "Disposing extension");
    }

    @Override
    public void initialize() {
        AndroidWebSocketLogger.i(this.tag, "Initialize");
    }

}
