package os;

public class OSSharedPageSource {
   public FSChannel channel;
   public int       pageNumber;

   public OSSharedPageSource(FSChannel channel, int pageNumber) {
    this.channel=channel;
    this.pageNumber=pageNumber;
   }
}
