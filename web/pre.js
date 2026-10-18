// Runs before the game starts; see the Emscripten section of the Makefile.

Module.preRun = Module.preRun || [];

// Keep the user directory (configuration and saved games) in IndexedDB so that it survives page reloads.
// file.c puts it at $HOME/.config/opentyrian2000, and Emscripten's $HOME is /home/web_user.  Changes are
// copied back to IndexedDB by webSyncUserFiles() in library.js.
(function () {
	var userDir = '/home/web_user/.config/opentyrian2000';
	var savesPath = userDir + '/tyrian.sav';

	var userFilesLoaded;
	// Resolves once the user directory has been loaded from IndexedDB.
	Module.userFilesReady = new Promise(function (resolve) { userFilesLoaded = resolve; });

	Module.preRun.push(function () {
		FS.mkdirTree(userDir);
		FS.mount(IDBFS, {}, userDir);

		addRunDependency('userFiles');
		FS.syncfs(true, function (error) {
			if (error)
				console.error('Failed to load user files from IndexedDB:', error);
			removeRunDependency('userFiles');
			userFilesLoaded();
			if (Module.onUserFilesReady)
				Module.onUserFilesReady();
		});
	});

	// Copies changes to the user directory to IndexedDB, then calls done(error) if given.  FS.syncfs must not
	// run more than once at a time, so requests made while a sync is running are combined into one more sync
	// afterwards.
	var syncing = false;
	var pending = null;
	Module.syncUserFilesToIndexedDB = function (done) {
		if (syncing) {
			(pending = pending || []).push(done);
			return;
		}
		syncing = true;
		var callbacks = [done];
		FS.syncfs(false, function finished(error) {
			if (error)
				console.error('Failed to save user files to IndexedDB:', error);
			callbacks.forEach(function (callback) { if (callback) callback(error); });
			if (pending) {
				callbacks = pending;
				pending = null;
				FS.syncfs(false, finished);
			} else {
				syncing = false;
			}
		});
	};

	// Saved games and high scores are in tyrian.sav, in the same format as Tyrian 2000's TYRIAN.SAV, so
	// they can be moved between this page, the desktop version, and the original game.

	function gameIsRunning() {
		return runtimeInitialized && !ABORT;
	}

	// Returns the saved games as a Uint8Array, or null if there are none yet.
	Module.exportSaves = function () {
		if (gameIsRunning())
			_webSaveUserFiles();  // make sure the file is up to date
		try {
			return FS.readFile(savesPath);
		} catch (error) {
			return null;
		}
	};

	// Checks that data is a Tyrian save file, the way the game does (decryptSaveData() in config.c).
	// Returns null if it is, or the reason it is not.
	Module.checkSaves = function (data) {
		var saveFileSize = 109 * 22 + 100;  // SAVE_FILE_SIZE in config.c
		var t2kHighScoresSize = 2220;  // T2K_HIGH_SCORES_SIZE in config.c

		if (data.length !== saveFileSize + 4 && data.length !== saveFileSize + 4 + t2kHighScoresSize)
			return 'This is not a Tyrian save file (it has the wrong size).';

		var cryptKey = [15, 50, 89, 240, 147, 34, 86, 9, 32, 208];
		var plain = data.slice(0, saveFileSize);
		for (var i = saveFileSize - 1; i >= 0; --i) {
			plain[i] ^= cryptKey[(i + 1) % 10];
			if (i > 0)
				plain[i] ^= data[i - 1];
		}

		var sum = 0, negativeSum = 0, product = 1, xor = 0;
		for (var j = 0; j < saveFileSize; ++j) {
			sum = (sum + plain[j]) & 0xff;
			negativeSum = (negativeSum - plain[j]) & 0xff;
			product = (product * plain[j] + 1) & 0xff;
			xor ^= plain[j];
		}
		if (data[saveFileSize] !== sum || data[saveFileSize + 1] !== negativeSum ||
		    data[saveFileSize + 2] !== product || data[saveFileSize + 3] !== xor)
			return 'This save file is damaged (its checksum does not match).';

		return null;
	};

	// Replaces the saved games with data (a Uint8Array) and stores them in IndexedDB.  Only possible while
	// the game is not running, because it would overwrite them with the ones it has loaded.  Returns a
	// Promise that is rejected with an Error whose message can be shown to the player.
	Module.importSaves = function (data) {
		return Module.userFilesReady.then(function () {
			if (gameIsRunning())
				throw new Error('Saved games can only be uploaded while the game is not running.');
			var problem = Module.checkSaves(data);
			if (problem)
				throw new Error(problem);
			FS.writeFile(savesPath, data);
			return new Promise(function (resolve, reject) {
				Module.syncUserFilesToIndexedDB(function (error) {
					if (error)
						reject(new Error('The saved games could not be stored in this browser.'));
					else
						resolve();
				});
			});
		});
	};
})();

// Save when the page is hidden, which also happens when it is closed (see webSaveUserFiles() in opentyr.c).
// This is a plain event listener because SDL uses Emscripten's visibilitychange callback for itself.
document.addEventListener('visibilitychange', function () {
	if (document.hidden && runtimeInitialized && !ABORT)
		_webSaveUserFiles();
});

// Browsers only allow audio after the user has interacted with the page, so the game does not start until
// the page calls Module.play() (from a click or key press).  Pages that don't need that can set
// Module.autoPlay = true.
(function () {
	var gestureNeeded = !Module.autoPlay;
	var waitingForGesture = false;

	Module.preRun.push(function () {
		if (gestureNeeded) {
			addRunDependency('userGesture');
			waitingForGesture = true;
		}
	});

	Module.play = function () {
		gestureNeeded = false;
		if (waitingForGesture) {
			waitingForGesture = false;
			removeRunDependency('userGesture');
		}
	};
})();
