"""Build a compact 8x8 RGBA icon fingerprint index from public tarkov.dev API data."""
import argparse
import concurrent.futures
import io
import json
import pathlib
import time
from urllib.request import Request, urlopen
from urllib.error import HTTPError
from PIL import Image

API = "https://api.tarkov.dev/graphql"
QUERY = "query { items(gameMode:pve, lang:ru) { id name width height gridImageLink } }"

def download(url, data=None, timeout=15):
    req = Request(url, data=data, headers={
        "User-Agent": "TarkovLootPrices/0.3",
        "Content-Type": "application/json"})
    try:
        with urlopen(req, timeout=timeout) as r:
            return r.read()
    except HTTPError as error:
        print('HTTP', error.code, 'from', url, error.read(4000).decode('utf-8', errors='replace'), flush=True)
        raise

def descriptor(row):
    if not row.get("gridImageLink"):
        return None
    try:
        raw = download(row["gridImageLink"])
        im = Image.open(io.BytesIO(raw)).convert("RGBA")
        w, h = im.size
        if w < 16 or h < 16:
            return None
        px = im.load()
        alpha, fg = [], []
        for y in range(8):
            for x in range(8):
                r, g, b, a = px[min(w-1, int((x+0.5)*w/8)), min(h-1, int((y+0.5)*h/8))]
                alpha.append(a)
                fg.append(round((0.299*r+0.587*g+0.114*b)*a/255))
        if sum(a > 100 for a in alpha) < 5:
            return None
        return {"id": row["id"], "width": row["width"], "height": row["height"], "fg": fg, "a": alpha}
    except (OSError, ValueError, TimeoutError) as e:
        print("skipping", row.get("id"), str(e))
        return None

def main():
    p = argparse.ArgumentParser()
    p.add_argument("--output", default="data/icons.json")
    p.add_argument("--workers", type=int, default=20)
    p.add_argument("--limit", type=int, default=0)
    args = p.parse_args()
    rows = json.loads(download(API, json.dumps({"query": QUERY}).encode())).get("data",{}).get("items",[])
    if args.limit > 0:
        rows = rows[:args.limit]
    print("Preparing", len(rows), "icon descriptors", flush=True)
    patterns=[]
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.workers) as pool:
        for i, d in enumerate(pool.map(descriptor, rows)):
            if d:
                patterns.append(d)
            if i%250==0:
                print(i, "processed,", len(patterns), "valid", flush=True)
    if len(patterns) < 200:
        raise RuntimeError("Only %d patterns built; refusing an empty index" % len(patterns))
    target=pathlib.Path(args.output)
    target.parent.mkdir(parents=True,exist_ok=True)
    target.write_text(json.dumps({"version":1,"generated":time.time(),"items":patterns},separators=(',',':')),encoding="utf-8")
    print("Wrote",len(patterns),"descriptors to",target)

if __name__=="__main__":
    main()
