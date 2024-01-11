declare interface noderc {
  hello(): string;
  version(): string;
  open(path: string): string;
  isFile(path: string): boolean;
  isDirectory(path: string): boolean;
  exists(path: string): boolean;
  diff(file: string, path: string): boolean;
}
declare const noderc: noderc;
export = noderc;
