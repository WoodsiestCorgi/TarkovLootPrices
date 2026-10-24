"""Temporary JSON schema diagnostic. No private data."""
import json
from urllib.request import urlopen
for path in ["pve/items","pve/items_ru"]:
    try:
        with urlopen("https://json.tarkov.dev/"+path,timeout=20) as r:
            root=json.load(r)
        print(path,"root keys:",list(root)[:12])
        data=root.get("data",root)
        print(path,"data type:",type(data).__name__,list(data)[:12] if isinstance(data,dict) else None)
        if path.endswith("_ru"):
            print(path,"data sample:",repr(list(data.items())[:2])[:2200] if isinstance(data,dict) else repr(data)[:2200])
        else:
            items=data.get("items")
            if isinstance(items,dict):items=list(items.values())
            print(path,"items type:",type(items).__name__,"length:",len(items))
            for item in [items[0],items[-1]]:
                print("item keys:",list(item))
                for key in ["id","name","shortName","avg24hPrice","lastLowPrice","sellFor","gridImageLink","updated","traderPrices","fleaMarket"]:
                    print(" field",key,repr(item.get(key))[:1200])
            print("root translations:",repr(root.get("translations"))[:400])
    except Exception as e:print(path,"ERROR",str(e))
