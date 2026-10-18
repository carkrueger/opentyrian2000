// Runs before the game starts; see the Emscripten section of the Makefile.

Module.preRun = Module.preRun || [];

// Keep the user directory (configuration and saved games) in IndexedDB so that it survives page reloads.
// file.c puts it at $HOME/.config/opentyrian2000, and Emscripten's $HOME is /home/web_user.  Changes are
// copied back to IndexedDB by webSyncUserFiles() in library.js.
Module.preRun.push(function () {
	var userDir = '/home/web_user/.config/opentyrian2000';

	FS.mkdirTree(userDir);
	FS.mount(IDBFS, {}, userDir);

	addRunDependency('userFiles');
	FS.syncfs(true, function (error) {
		if (error)
			console.error('Failed to load user files from IndexedDB:', error);
		removeRunDependency('userFiles');
	});
});

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
