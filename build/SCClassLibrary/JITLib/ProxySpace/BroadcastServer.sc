

BroadcastServer : Server {

	var <>homeServer, <>addresses;
 
	init { arg argName, argAddr, options, clientID;
		homeServer = Server.new(argName.asSymbol, argAddr, options, clientID ? 0);
		name = (argName ++ "_broadcast").asSymbol;
		addr = argAddr;
		inProcess = homeServer.inProcess;
	}
	
	
	*for { arg homeServer, addresses; //local server is a router
			^super.new(homeServer.name)
				.homeServer_(homeServer)
				.addresses_(addresses)
	}
	
	addr { ^addresses } // support NodeWatcher.register
		
	// iterating
	
	at { arg index;
		^addresses.clipAt(index)
	}
	
	wrapAt { arg index;
		^addresses.wrapAt(index)
	}
	
	do { arg function;
		^addresses.do(function)
	}
	
	
	// message forwarding
		
	sendMsg { arg ... args;
		addresses.do({ arg addr; addr.sendMsg(*args) });
	}
	sendBundle { arg time ... messages;
		addresses.do({ arg addr; addr.sendBundle(time, *messages); });
	}
	sendRaw { arg rawArray;
		addresses.do({ arg addr; addr.sendRaw(rawArray) })
	}
	
	listSendMsg { arg msg;
		addresses.do({ arg addr; addr.sendMsg(*msg) });
	}
 	listSendBundle { arg time,bundle;
 		addresses.do({ arg addr; addr.sendBundle(time, *bundle) })
	}
	
	// need to verride some methods in superclass
	
	options { ^homeServer.options }
	
	makeWindow { homeServer.makeWindow }
	
	boot { homeServer.waitForBoot { this.notify; this.initTree; } }

	serverRunning { ^homeServer.serverRunning }
	serverBooting { ^homeServer.serverBooting }
	
	status {
		this.sendMsg("/status");
	}
	notify { arg flag=true;
		this.sendMsg("/notify", flag.binaryValue);
	}
	
	// todo:
	// wait
	// openBundle
	// makeBundle
	

	
	
	// sync
	sendMsgSync { arg condition ... args;
		var cmdName, count;
		if (condition.isNil) { condition = Condition.new };
		condition.test = false;
		count = addresses.size;
		cmdName = args[0].asString;
		if (cmdName[0] != $/) { cmdName = cmdName.insert(0, $/) };
		addresses.do { arg addr;
			var resp;
			resp = OSCresponderNode(addr, "/done", {|time, resp, msg|
			if (msg[1].asString == cmdName) {
				resp.remove;
				count = count - 1;
				if(count == 0) {
					condition.test = true;
					condition.signal;
				};			
			};
			}).add;
		
			addr.sendBundle(nil, args);
		};
		condition.wait;
	}
	
	sync { arg condition, bundles, latency; // array of bundles that cause async action
		var count;
		if (condition.isNil) { condition = Condition.new };
		count = addresses.size;
		condition.test = false;
		addresses.do { arg addr;
			var resp, id;
			id = UniqueID.next;
			resp = OSCresponderNode(addr, "/synced", {|time, resp, msg|
			if (msg[1] == id) {
				resp.remove;
				count = count - 1;
				if(count == 0) {
					condition.test = true;
					condition.signal;
				};
			};
			}).add;
			if(bundles.isNil) {
				addr.sendBundle(latency, ["/sync", id]);
			} {
				addr.sendBundle(latency, *(bundles ++ [["/sync", id]]));
			};
		};
		condition.wait;
	}
	
	
		
	// use home server allocators

	nodeAllocator { ^homeServer.nodeAllocator }
	staticNodeAllocator { ^homeServer.staticNodeAllocator }
	controlBusAllocator { ^homeServer.controlBusAllocator }
	audioBusAllocator { ^homeServer.audioBusAllocator }
	bufferAllocator { ^homeServer.bufferAllocator }
	
	nextNodeID { ^homeServer.nodeAllocator.alloc }
	nextSharedNodeID { ^homeServer.nextSharedNodeID }
	
	
}

