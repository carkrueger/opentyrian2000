// JavaScript functions called from C; see the Emscripten section of the Makefile.

addToLibrary({
	// Copies changes to the user directory to IndexedDB (see pre.js).  FS.syncfs must not run more than once
	// at a time, so requests made while a sync is running are combined into one more sync afterwards.
	webSyncUserFiles__deps: ['$FS'],
	webSyncUserFiles: function () {
		if (Module.userFilesSyncing) {
			Module.userFilesSyncPending = true;
			return;
		}

		Module.userFilesSyncing = true;

		var done = function (error) {
			if (error)
				console.error('Failed to save user files to IndexedDB:', error);

			if (Module.userFilesSyncPending) {
				Module.userFilesSyncPending = false;
				FS.syncfs(false, done);
			} else {
				Module.userFilesSyncing = false;
			}
		};

		FS.syncfs(false, done);
	},
});
