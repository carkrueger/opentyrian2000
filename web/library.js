// JavaScript functions called from C; see the Emscripten section of the Makefile.

addToLibrary({
	// Copies changes to the user directory to IndexedDB (see pre.js).
	webSyncUserFiles: function () {
		Module.syncUserFilesToIndexedDB();
	},
});
