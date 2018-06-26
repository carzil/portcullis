from portcullis import Splicer

class Handler:
    def __init__(self, ctx, client, backend):
        self.ctx = ctx
        self.client = client
        self.backend = backend
        self.splicer = Splicer(self.ctx, self.client, self.backend, self.end)
        self.ctx.start_splicer(self.splicer)

    def end(self):
        pass
