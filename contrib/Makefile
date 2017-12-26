

.PHONY: domake libwebsockets clean



domake: libwebsockets

libwebsockets:
	-mkdir ./libwebsockets/build
	cd ./libwebsockets/build;\
	cmake .. -DLWS_WITH_SSL=OFF;\
	make


clean:
	rm -rf ./libwebsockets/build
