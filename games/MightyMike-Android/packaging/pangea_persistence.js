(function () {
  function mountPangeaPersistence(module) {
    if (!module.FS || !module.FS.filesystems || !module.FS.filesystems.IDBFS) return;
    try { module.FS.mkdir('/Data/Scripts/persistence'); } catch (error) { /* exists */ }
    try {
      module.FS.mount(module.FS.filesystems.IDBFS, { autoPersist: true }, '/Data/Scripts/persistence');
      module.addRunDependency('pangea-persistence-sync');
      module.FS.syncfs(true, function (error) {
        if (error) console.error('[Pangea] persistence sync failed', error);
        module.removeRunDependency('pangea-persistence-sync');
      });
    } catch (error) { console.error('[Pangea] persistence mount failed', error); }
  }

  Module = Module || {};
  Module.preRun = Module.preRun || [];
  Module.preRun.push(function () { mountPangeaPersistence(Module); });
}());
