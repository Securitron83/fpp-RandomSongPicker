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
                Pick a random, non-recently-played song from a source playlist.
                Two commands are available: one that plays the picked song immediately as the next
                item in the calling playlist, and one that writes the pick to an output playlist
                for a separate Playlist entry to play.
            </p>
        </div>
    </div>

    <!-- Insert Random Item with History -->
    <div class="card card-outline card-warning mt-3">
        <div class="card-header">
            <h3 class="card-title"><i class="fas fa-play-circle"></i> Insert Random Item with History</h3>
        </div>
        <div class="card-body">
            <p class="text-muted small mb-3">
                Picks a random song and immediately queues it as the next item in the calling playlist.
                FPP plays it, then the calling playlist resumes — Lead Out still runs.
            </p>

            <div class="form-group row mb-2">
                <label class="col-sm-3 col-form-label col-form-label-sm"><strong>Source Playlist</strong></label>
                <div class="col-sm-9">
                    <select id="insertSourceName" class="form-control form-control-sm" style="max-width:280px">
                        <option value="">— loading playlists… —</option>
                    </select>
                    <small class="text-muted">Playlist to pick a random song from</small>
                </div>
            </div>

            <div class="form-group row mb-2">
                <label class="col-sm-3 col-form-label col-form-label-sm"><strong>History Size</strong></label>
                <div class="col-sm-9">
                    <input type="number" id="insertHistorySize" class="form-control form-control-sm"
                           style="max-width:100px" min="1" max="100" value="10">
                    <small class="text-muted">Songs to exclude before repeating</small>
                </div>
            </div>

            <div class="form-group row">
                <div class="col-sm-9 offset-sm-3">
                    <button class="btn btn-warning btn-sm" onclick="doInsert()">
                        <i class="fas fa-play-circle"></i> Pick &amp; Play Now
                    </button>
                    <span id="insertStatus" class="ml-3 small"></span>
                </div>
            </div>

        </div>
    </div>

    <!-- Write Random Item to Playlist -->
    <div class="card card-outline card-info mt-3">
        <div class="card-header">
            <h3 class="card-title"><i class="fas fa-file-alt"></i> Write Random Item to Playlist</h3>
        </div>
        <div class="card-body">
            <p class="text-muted small mb-3">
                Picks a random song and writes it to an output playlist. Does not start playback —
                add a separate Playlist entry after this command to play the output playlist.
            </p>

            <div class="form-group row mb-2">
                <label class="col-sm-3 col-form-label col-form-label-sm"><strong>Source Playlist</strong></label>
                <div class="col-sm-9">
                    <select id="writeSourceName" class="form-control form-control-sm" style="max-width:280px">
                        <option value="">— loading playlists… —</option>
                    </select>
                    <small class="text-muted">Playlist to pick a random song from</small>
                </div>
            </div>

            <div class="form-group row mb-2">
                <label class="col-sm-3 col-form-label col-form-label-sm"><strong>Output Playlist</strong></label>
                <div class="col-sm-9">
                    <select id="writeOutputSelect" class="form-control form-control-sm" style="max-width:280px"
                            onchange="toggleNewPlaylist()">
                        <option value="">— loading playlists… —</option>
                    </select>
                    <div id="newPlaylistRow" style="display:none;margin-top:6px">
                        <input type="text" id="newPlaylistName" class="form-control form-control-sm"
                               style="max-width:280px" placeholder="New playlist name (without .json)">
                    </div>
                    <small class="text-muted">Single-song playlist that will be written</small>
                </div>
            </div>

            <div class="form-group row mb-2">
                <label class="col-sm-3 col-form-label col-form-label-sm"><strong>History Size</strong></label>
                <div class="col-sm-9">
                    <input type="number" id="writeHistorySize" class="form-control form-control-sm"
                           style="max-width:100px" min="1" max="100" value="10">
                    <small class="text-muted">Songs to exclude before repeating</small>
                </div>
            </div>

            <div class="form-group row mb-2">
                <label class="col-sm-3 col-form-label col-form-label-sm"><strong>Replace Previous</strong></label>
                <div class="col-sm-9 d-flex align-items-center mt-1">
                    <div class="form-check">
                        <input class="form-check-input" type="checkbox" id="replacePrevious" checked>
                        <label class="form-check-label" for="replacePrevious">
                            Replace the previous item instead of appending
                            <small class="text-muted d-block">Unchecked: append after the previous item</small>
                        </label>
                    </div>
                </div>
            </div>

            <div class="form-group row">
                <div class="col-sm-9 offset-sm-3">
                    <button class="btn btn-info btn-sm" onclick="doWrite()">
                        <i class="fas fa-file-alt"></i> Write Now
                    </button>
                    <button class="btn btn-outline-secondary btn-sm ml-2" onclick="loadPlaylists()">
                        <i class="fas fa-sync-alt"></i> Refresh Lists
                    </button>
                    <span id="writeStatus" class="ml-3 small"></span>
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
            <p class="mb-2">Three FPP commands are registered, available under <strong>Sequences &rarr; Command Presets</strong> and in any playlist Command entry:</p>
            <table class="table table-sm table-bordered" style="max-width:780px">
                <thead class="thead-light">
                    <tr><th>Command</th><th>Arguments</th><th>Use when…</th></tr>
                </thead>
                <tbody>
                    <tr>
                        <td><code>Insert Random Item with History</code></td>
                        <td>
                            <strong>Source Playlist</strong> — playlist to pick from (required)<br>
                            <strong>History Size</strong> — songs to skip before repeating, default: <em>10</em>
                        </td>
                        <td>You want the command to pick <em>and</em> play immediately. Put this command in Lead In or Main; FPP plays the picked item, then resumes the calling playlist including Lead Out.</td>
                    </tr>
                    <tr>
                        <td><code>Write Random Item to Playlist</code></td>
                        <td>
                            <strong>Source Playlist</strong> — playlist to pick from (required)<br>
                            <strong>Output Playlist</strong> — default: <em>RandomPick</em><br>
                            <strong>History Size</strong> — default: <em>10</em><br>
                            <strong>Replace Previous</strong> — overwrite or append, default: <em>true</em><br>
                            <small class="text-muted">Lead In and Lead Out sections are always preserved.</small>
                        </td>
                        <td>You want to write the pick to a playlist and play it with a separate Playlist entry. Useful when you need Lead In/Out on the output playlist itself.</td>
                    </tr>
                    <tr>
                        <td><code>Clear Playlist Main</code></td>
                        <td>
                            <strong>Playlist</strong> — playlist whose main section to clear (required)<br>
                            <small class="text-muted">Clears only the main items; Lead In and Lead Out are left intact.</small>
                        </td>
                        <td>Reset an output playlist between shows.</td>
                    </tr>
                </tbody>
            </table>
            <p class="text-muted small mb-1">
                <strong>Typical simple setup (Insert command):</strong> Scheduler playlist Lead In = WLED on,
                Main = <code>Insert Random Item with History</code> (Source = your song library),
                Lead Out = WLED off. The plugin picks a random, non-recently-played song and FPP plays it
                inline before running Lead Out.
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
    $('#insertSourceName').html('<option value="">— loading… —</option>');
    $('#writeSourceName').html('<option value="">— loading… —</option>');
    $('#writeOutputSelect').html('<option value="">— loading… —</option>');

    fetch('/api/files/playlists')
        .then(r => r.ok ? r.json() : Promise.reject(r.status))
        .then(data => {
            var files = (data.files || [])
                .map(f => f.name || f)
                .filter(n => n.endsWith('.json'))
                .map(n => n.replace(/\.json$/i, ''))
                .sort();

            playlistNames = files;

            var srcHtml = '<option value="">— select source playlist —</option>';
            files.forEach(function(n) {
                srcHtml += '<option value="' + escHtml(n) + '">' + escHtml(n) + '</option>';
            });
            $('#insertSourceName').html(srcHtml);
            $('#writeSourceName').html(srcHtml);

            var outHtml = '';
            files.forEach(function(n) {
                var sel = (n === 'RandomPick') ? ' selected' : '';
                outHtml += '<option value="' + escHtml(n) + '"' + sel + '>' + escHtml(n) + '</option>';
            });
            outHtml += '<option value="__new__">— Create new playlist… —</option>';
            if (files.indexOf('RandomPick') === -1) {
                outHtml = '<option value="__new__" selected>— Create new playlist… —</option>' + outHtml;
                $('#newPlaylistName').val('RandomPick');
                $('#newPlaylistRow').show();
            }
            $('#writeOutputSelect').html(outHtml);
            toggleNewPlaylist();
        })
        .catch(function() {
            $('#insertSourceName').html('<option value="">— failed to load playlists —</option>');
            $('#writeSourceName').html('<option value="">— failed to load playlists —</option>');
            $('#writeOutputSelect').html('<option value="__new__">— Create new playlist… —</option>');
            $('#newPlaylistRow').show();
            setStatus('insertStatus', false, 'Could not load playlist list from FPP API');
        });
}

function toggleNewPlaylist() {
    if ($('#writeOutputSelect').val() === '__new__') {
        $('#newPlaylistRow').show();
    } else {
        $('#newPlaylistRow').hide();
    }
}

function getOutputName() {
    var sel = $('#writeOutputSelect').val();
    if (sel === '__new__') {
        return $('#newPlaylistName').val().trim() || '';
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

function setStatus(id, ok, msg) {
    $('#' + id).html(ok
        ? '<span class="text-success"><i class="fas fa-check"></i> ' + msg + '</span>'
        : '<span class="text-danger"><i class="fas fa-times"></i> ' + msg + '</span>'
    );
}

function doInsert() {
    var src  = $('#insertSourceName').val();
    var hist = $('#insertHistorySize').val() || '10';

    if (!src) { setStatus('insertStatus', false, 'Select a source playlist'); return; }

    setStatus('insertStatus', true, 'Picking…');
    apiCommand('Insert Random Item with History', [src, hist])
        .then(function(r) { setStatus('insertStatus', true, r.result || 'Done'); })
        .catch(function() { setStatus('insertStatus', false, 'Failed — is the plugin loaded?'); });
}

function doWrite() {
    var src     = $('#writeSourceName').val();
    var out     = getOutputName();
    var hist    = $('#writeHistorySize').val() || '10';
    var replace = $('#replacePrevious').is(':checked') ? 'true' : 'false';

    if (!src) { setStatus('writeStatus', false, 'Select a source playlist'); return; }
    if (!out) { setStatus('writeStatus', false, 'Enter a name for the output playlist'); return; }

    setStatus('writeStatus', true, 'Picking…');
    apiCommand('Write Random Item to Playlist', [src, out, hist, replace])
        .then(function(r) {
            setStatus('writeStatus', true, r.result || 'Done');
            loadPlaylists();
        })
        .catch(function() { setStatus('writeStatus', false, 'Failed — is the plugin loaded?'); });
}

$(document).ready(function() {
    loadPlaylists();
});
</script>
