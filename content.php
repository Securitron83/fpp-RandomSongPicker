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
                    <select id="sourceName" class="form-control form-control-sm" style="max-width:280px">
                        <option value="">— loading playlists… —</option>
                    </select>
                    <small class="text-muted">Playlist to pick a random song from</small>
                </div>
            </div>

            <div class="form-group row mb-2">
                <label class="col-sm-3 col-form-label col-form-label-sm"><strong>Output Playlist</strong></label>
                <div class="col-sm-9">
                    <select id="outputSelect" class="form-control form-control-sm" style="max-width:280px"
                            onchange="toggleNewPlaylist()">
                        <option value="">— loading playlists… —</option>
                    </select>
                    <div id="newPlaylistRow" style="display:none;margin-top:6px">
                        <input type="text" id="newPlaylistName" class="form-control form-control-sm"
                               style="max-width:280px" placeholder="New playlist name (without .json)">
                    </div>
                    <small class="text-muted">Single-song playlist that will be written and played</small>
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
                <div class="col-sm-9 d-flex align-items-center mt-1">
                    <div class="form-check form-check-inline">
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
                    <button class="btn btn-outline-secondary btn-sm ml-2" onclick="loadPlaylists()">
                        <i class="fas fa-sync-alt"></i> Refresh Lists
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
var playlistNames = [];

function loadPlaylists() {
    $('#sourceName').html('<option value="">— loading… —</option>');
    $('#outputSelect').html('<option value="">— loading… —</option>');

    fetch('/api/files/playlists')
        .then(r => r.ok ? r.json() : Promise.reject(r.status))
        .then(data => {
            var files = (data.files || [])
                .map(f => f.name || f)
                .filter(n => n.endsWith('.json'))
                .map(n => n.replace(/\.json$/i, ''))
                .sort();

            playlistNames = files;

            // Source dropdown — all playlists
            var srcHtml = '<option value="">— select source playlist —</option>';
            files.forEach(function(n) {
                srcHtml += '<option value="' + escHtml(n) + '">' + escHtml(n) + '</option>';
            });
            $('#sourceName').html(srcHtml);

            // Output dropdown — all playlists + create new option
            var outHtml = '';
            files.forEach(function(n) {
                // Pre-select RandomPick if it exists
                var sel = (n === 'RandomPick') ? ' selected' : '';
                outHtml += '<option value="' + escHtml(n) + '"' + sel + '>' + escHtml(n) + '</option>';
            });
            outHtml += '<option value="__new__">— Create new playlist… —</option>';
            // If RandomPick doesn't exist, add it as default new option pre-filled
            if (files.indexOf('RandomPick') === -1) {
                outHtml = '<option value="__new__" selected>— Create new playlist… —</option>' + outHtml;
                $('#newPlaylistName').val('RandomPick');
                $('#newPlaylistRow').show();
            }
            $('#outputSelect').html(outHtml);
            toggleNewPlaylist();
        })
        .catch(function() {
            $('#sourceName').html('<option value="">— failed to load playlists —</option>');
            $('#outputSelect').html('<option value="__new__">— Create new playlist… —</option>');
            $('#newPlaylistRow').show();
            setStatus(false, 'Could not load playlist list from FPP API');
        });
}

function toggleNewPlaylist() {
    if ($('#outputSelect').val() === '__new__') {
        $('#newPlaylistRow').show();
    } else {
        $('#newPlaylistRow').hide();
    }
}

function getOutputName() {
    var sel = $('#outputSelect').val();
    if (sel === '__new__') {
        var n = $('#newPlaylistName').val().trim();
        return n || '';
    }
    return sel || '';
}

function escHtml(s) {
    return s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;');
}

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
    var src  = $('#sourceName').val();
    var out  = getOutputName();
    var hist = $('#historySize').val() || '10';
    var play = $('input[name="startPlayback"]:checked').val();

    if (!src) { setStatus(false, 'Select a source playlist'); return; }
    if (!out) { setStatus(false, 'Enter a name for the new playlist'); return; }

    setStatus(true, 'Picking…');
    apiCommand('Playlist - Pick Random Song', [src, out, hist, play])
        .then(function(r) {
            setStatus(true, r.result || 'Done');
            // Refresh list in case a new playlist was created
            loadPlaylists();
        })
        .catch(function() { setStatus(false, 'Failed — is the plugin loaded?'); });
}

$(document).ready(function() {
    loadPlaylists();
});
</script>
