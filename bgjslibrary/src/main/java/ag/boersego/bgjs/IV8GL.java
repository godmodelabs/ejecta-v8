package ag.boersego.bgjs;

public interface IV8GL {

	public void requestRender();

    public void setInteractive(boolean b);

	public void unpause();

	public void pause();
	
    public void setRenderCallback (IV8GLViewOnRender listener);
	
	public interface IV8GLViewOnRender {
		public void renderStarted(int chartId);
		public void renderThreadClosed(int chartId);
	}
}
