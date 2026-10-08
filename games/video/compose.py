"""compose.py <game 1..5> : frames + bands -> out/<name>.mp4 (1280x720, 30 fps, H.264, X-friendly)."""
import json, os, subprocess, sys
H = os.path.dirname(os.path.abspath(__file__))
gid = int(sys.argv[1]); d = os.path.join(H, 'out', f'g{gid}')
NAMES = {1: '1-wildfire-isle', 2: '2-tavern-tables', 3: '3-colony-ledger', 4: '4-dungeon-daily', 5: '5-slime-lab'}
t = json.load(open(os.path.join(d, 'timeline.json')))
frames, caps, dur = t['frames'], t['caps'], t['dur']
# variable-frame-rate concat list from the screencast timestamps
with open(os.path.join(d, 'frames.txt'), 'w') as f:
    for k, (name, ts) in enumerate(frames):
        nxt = frames[k + 1][1] if k + 1 < len(frames) else dur
        f.write(f"file 'frames/{name}'\nduration {max(nxt - ts, 0.001):.4f}\n")
    f.write(f"file 'frames/{frames[-1][0]}'\n")
W, TOP, BOT = 1280, 120, 96
MID = 720 - TOP - BOT
inputs = ['-f', 'concat', '-safe', '0', '-i', 'frames.txt', '-loop', '1', '-i', 'top.png']
for k in range(len(caps)): inputs += ['-loop', '1', '-i', f'cap{k}.png']
fc = [f"[0:v]fps=30,scale=w={W}:h={MID}:force_original_aspect_ratio=decrease:flags=lanczos,"
      f"pad={W}:{MID}:(ow-iw)/2:(oh-ih)/2:color=0x0f0e0c,setsar=1[game]",
      f"color=c=0x0f0e0c:s={W}x720:r=30:d={dur}[bg]",
      f"[1:v]scale={W}:{TOP}:flags=lanczos[top]",
      f"[bg][game]overlay=0:{TOP}:shortest=1[a0]",
      f"[a0][top]overlay=0:0:shortest=1[b0]"]
last = 'b0'
for k, (ts, ci) in enumerate(caps):
    end = caps[k + 1][0] if k + 1 < len(caps) else dur + 1
    fc.append(f"[{2 + k}:v]scale={W}:{BOT}:flags=lanczos[c{k}]")
    fc.append(f"[{last}][c{k}]overlay=0:{TOP + MID}:enable='between(t,{ts},{end - 0.001})':shortest=1[o{k}]")
    last = f'o{k}'
out = os.path.join(H, 'out', NAMES[gid] + '.mp4')
cmd = ['ffmpeg', '-y', '-loglevel', 'error', *inputs, '-filter_complex', ';'.join(fc), '-map', f'[{last}]', '-t', str(dur),
       '-c:v', 'libx264', '-preset', 'slow', '-crf', '20', '-pix_fmt', 'yuv420p', '-r', '30', '-movflags', '+faststart', out]
subprocess.run(cmd, cwd=d, check=True)
print(out, round(os.path.getsize(out) / 1e6, 2), 'MB')
