<?php
// No server-side state needed — all communication goes through FPP's command API.
?>

<div class="container-fluid">

    <!-- Overview -->
    <div class="card card-outline card-primary">
        <div class="card-header">
            <h3 class="card-title"><i class="fas fa-random"></i> Random Song Picker</h3>
        </div>
        <div class="card-body">
            <p class="text-muted small mb-0">
                Pick a random, non-recently-played song from a source playlist and write it to
                a single-song output playlist. Add the <strong>Playlist - Pick Random Song</strong>
                command to any FPP playlist to replace cron-based random selection — no cron job needed.
            </p>
        </div>
    </div>

    <!-- Test / Manual Pick -->
    <div class="card card-outline card-warning mt-3">
        <div class="card-header">
            <h3 class="card-title"><i class="fas fa-dice"></i> Manual Pick</h3>
        </div>
        <div class="card-body">
            <div class="form-group row mb-2">
                <label class="col-sm-3 col-form-label col-form-label-sm"><strong>Source Playlist</strong></label>
                <div class="col-sm-9">
                    <input type="text" id="sourceName" class="form-control form-control-sm"
                           style="max-width:280px" placeholder="e.g. Christmas">
                    <small class="text-muted">Name without .json</small>
                </div>
            </div>
            <div class="form-group row mb-2">
                <label class="col-sm-3 col-form-label col-form-label-sm"><strong>Output Playlist</strong></label>
                <div class="col-sm-9">
                    <input type="text" id="outputName" class="form-control form-control-sm"
                           style="max-width:280px" value="RandomPick">
                    <small class="text-muted">Name without .json</small>
                </div>
            </div>
            <div class="form-group row mb-2">
                <label class="col-sm-3 col-form-label col-form-label-sm"><strong>History Size</strong></label>
                <div class="col-sm-9">
                    <input type="number" id="historySize" class="form-control form-control-sm"
                           style="max-width:100px" min="1" max="100" value="10">
                    <small class="text-muted">Songs to exclude before repeating</small>
                </div>
            </div>
            <div class="form-group row mb-2">
                <label class="col-sm-3 col-form-label col-form-label-sm"><strong>Start Playback</strong></label>
                <div class="col-sm-9 d-flex align-items-center">
                    <div class="form-check form-check-inline mt-1">
                        <input class="form-check-input" type="radio" name="startPlayback" id="spYes" value="Yes" checked>
                        <label class="form-check-label" for="spYes">Yes</label>
                    </div>
                    <div class="form-check form-check-inline">
                        <input class="form-check-input" type="radio" name="startPlayback" id="spNo" value="No">
                        <label class="form-check-label" for="spNo">No (just write playlist)</label>
                    </div>
                </div>
            </div>
            <div class="form-group row">
                <div class="col-sm-9 offset-sm-3">
                    <button class="btn btn-warning btn-sm" onclick="doPick()">
                        <i class="fas fa-dice"></i> Pick Now
                    </button>
                    <span id="pickStatus" class="ml-3 small"></span>
                </div>
            </div>
        </div>
    </div>

    <!-- How to use -->
    <div class="card card-outline card-secondary mt-3">
        <div class="card-header">
            <h3 class="card-title"><i class="fas fa-info-circle"></i> Using in Playlists</h3>
            <div class="card-tools">
                <button type="button" class="btn btn-tool" data-card-widget="collapse">
                    <i class="fas fa-minus"></i>
                </button>
            </div>
        </div>
        <div class="card-body">
            <p class="mb-2">One FPP command is available under <strong>Sequences &rarr; Command Presets</strong>:</p>
            <table class="table table-sm table-bordered" style="max-width:780px">
                <thead class="thead-light">
                    <tr><th>Command</th><th>Arguments</th></tr>
                </thead>
                <tbody>
                    <tr>
                        <td><code>Playlist - Pick Random Song</code></td>
                        <td>
                            <strong>Source Playlist</strong> — name without .json (required)<br>
                            <strong>Output Playlist</strong> — default: <em>RandomPick</em><br>
                            <strong>History Size</strong> — songs to skip before repeating, default: <em>10</em><br>
                            <strong>Start Playback</strong> — <em>Yes</em> / <em>No</em>, default: <em>Yes</em>
                        </td>
                    </tr>
                </tbody>
            </table>
            <p class="text-muted small mb-1">
                <strong>Typical setup:</strong> Create a master playlist (e.g. <em>Christmas</em>) containing
                all your songs. Add a <em>Command</em> entry at the top of your show Lead-In that fires
                <code>Playlist - Pick Random Song</code> with Source&nbsp;=&nbsp;<em>Christmas</em>.
                The plugin writes <em>RandomPick.json</em> and starts it — a different song each time,
                with no repeats until the full list has been played through.
            </p>
            <p class="text-muted small mb-0">
                History is stored per source playlist in
                <code>/home/fpp/media/logs/song_picker_&lt;source&gt;_history.txt</code>.
                Delete this file to reset the history.
            </p>
        </div>
    </div>

</div>

<script>
function apiCommand(cmd, args) {
    const parts = ['/api/command', encodeURIComponent(cmd)].concat(args.map(encodeURIComponent));
    return fetch(parts.join('/'), { method: 'GET' })
        .then(r => r.ok ? r.json() : Promise.reject(r.status));
}

function setStatus(ok, msg) {
    $('#pickStatus').html(ok
        ? '<span class="text-success"><i class="fas fa-check"></i> ' + msg + '</span>'
        : '<span class="text-danger"><i class="fas fa-times"></i> ' + msg + '</span>'
    );
}

function doPick() {
    const src  = $('#sourceName').val().trim();
    const out  = $('#outputName').val().trim() || 'RandomPick';
    const hist = $('#historySize').val() || '10';
    const play = $('input[name="startPlayback"]:checked').val();

    if (!src) { setStatus(false, 'Enter a source playlist name'); return; }
    setStatus(true, 'Picking…');
    apiCommand('Playlist - Pick Random Song', [src, out, hist, play])
        .then(r => setStatus(true, r.result || 'Done'))
        .catch(() => setStatus(false, 'Failed — is the plugin loaded?'));
}
</script>
