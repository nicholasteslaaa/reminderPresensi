from fastapi import FastAPI, Form
from fastapi.middleware.cors import CORSMiddleware
import asyncio
import uvicorn
from bot import Bot

app = FastAPI()

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)

bots: dict[str, Bot] = {}

# -------------------------
# Background scheduler
# -------------------------
async def worker():
    while True:
        print("Worker alive | Bots:", list(bots.keys()))
        await asyncio.sleep(1)

@app.on_event("startup")
async def startup():
    asyncio.create_task(worker())


@app.post("/addBot")
def add_bot(id: str = Form(...), password: str = Form(...)):
    if id in bots:
        return {"status": "Bot already exists"}

    bot = Bot(id, password)
    if not bot.checkLogin():
        return {"status": "Login failed"}

    bots[id] = bot
    bot.start()  # THREAD START
    return {"status": "Bot started"}


@app.post("/stopBot")
def stop_bot(id: str = Form(...)):
    if id in bots:
        bots[id].stop()
        bots.pop(id)
        return {"status": "Stopped"}
    return {"status": "Not found"}


if __name__  == "__main__":
    uvicorn.run(
        "API:app",        # filename:app
        host="127.0.0.1",
        port=8000,
        reload=True        # auto-reload (dev only)
    )