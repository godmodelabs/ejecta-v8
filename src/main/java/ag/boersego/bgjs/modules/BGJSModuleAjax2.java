package ag.boersego.bgjs.modules;

import java.util.concurrent.ThreadPoolExecutor;

import ag.boersego.bgjs.JNIV8Function;
import ag.boersego.bgjs.JNIV8GenericObject;
import ag.boersego.bgjs.JNIV8Module;
import ag.boersego.bgjs.JNIV8Object;
import ag.boersego.bgjs.JNIV8Undefined;
import ag.boersego.bgjs.V8Engine;
import ag.boersego.bgjs.data.V8UrlCache;
import okhttp3.OkHttpClient;

/**
 * Created by Kevin Read <me@kevin-read.com> on 17.11.17 for myrmecophaga-2.0.
 * Copyright (c) 2017 BörseGo AG. All rights reserved.
 */

public class BGJSModuleAjax2 extends JNIV8Module {

    private static final BGJSModuleAjax2 sInstance = new BGJSModuleAjax2();
    private V8UrlCache mCache;
    private ThreadPoolExecutor executor;
    private OkHttpClient httpClient;

    public static BGJSModuleAjax2 getInstance() {
        return sInstance;
    }

    private BGJSModuleAjax2() {
        super("ajax");
    }

    public void setUrlCache (V8UrlCache cache) {
        mCache = cache;
    }

    @Override
    public void Require(V8Engine engine, JNIV8GenericObject module) {
        final JNIV8GenericObject exports = JNIV8GenericObject.Create(engine);

        exports.setV8Field("request", JNIV8Function.Create(engine, (receiver, arguments) -> {
            if (arguments.length < 2) {
                throw new RuntimeException("request: Requires at least two arguments");
            }
            if (!(arguments[0] instanceof String)) {
                throw new RuntimeException("request: first argument is url and must be a String");
            }
            final String url = (String) arguments[0];

            final String method;
            if (arguments[1] == null || arguments[1] instanceof JNIV8Undefined) {
                method = "GET";
            } else {
                if (!(arguments[1] instanceof String)) {
                    throw new RuntimeException("request: second argument is method and must be a String of the form [GET|POST|DELETE|HEAD|PUT]");
                }
            method = (String) arguments[1];
            }
            final JNIV8GenericObject headers;

            if (arguments.length >= 3) {
                if (arguments[2] instanceof JNIV8GenericObject ) {
                    headers = (JNIV8GenericObject) arguments[2];
                } else if (arguments[2] != null && !(arguments[2] instanceof JNIV8Undefined)) {
                    throw new RuntimeException("request: third argument must be undefined or an object and not " + arguments[2]);
                } else {
                    headers = null;
                }
            } else {
                headers = null;
            }

            final Object body;
            int timeoutMs = -1;
            if (arguments.length >= 4) {
                if (arguments[3] == null || arguments[3] instanceof String) {
                    body = arguments[3];
                } else if (arguments[3] instanceof JNIV8Object) {
                    body = arguments[3];
                } else if (!(arguments[3] instanceof JNIV8Undefined)) {
                    throw new RuntimeException("request: fourth argument is body and must be undefined or a String");
                } else {
                    body = null;
                }
                if (arguments.length >= 5) {
                    if (arguments[4] instanceof Number) {
                        timeoutMs = ((Number)arguments[4]).intValue();
                    } else if (arguments[4] instanceof String) {
                        timeoutMs = Integer.valueOf((String)arguments[4]);
                    }
                }
            } else {
                body = null;
            }




            final BGJSModuleAjax2Request request = new BGJSModuleAjax2Request(engine);
            request.setData(url, method, headers, body, mCache, httpClient, executor, timeoutMs);

            engine.enqueueOnNextTick(request);

            return request;
        }));

        module.setV8Field("exports", exports);
    }

    public void setExecutor(ThreadPoolExecutor executor) {
        this.executor = executor;
    }

    public void setHttpClient(OkHttpClient httpClient) {
        this.httpClient = httpClient;
    }
}
